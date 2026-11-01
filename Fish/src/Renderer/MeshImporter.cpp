#include "fspch.h"
#include "MeshImporter.h"
#include "Renderer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cctype>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace Fish {
	namespace {
		struct MeshCandidate
		{
			std::vector<MeshVertex> vertices;
			std::vector<uint32_t>   indices;
			MeshId                  id = k_InvalidMeshId;
		};

		// aiMatrix4x4 行主序存储,glm::mat4 列主序;直接 memcpy 会得到转置。
		glm::mat4 ToGlm(const aiMatrix4x4& m)
		{
			glm::mat4 result;
			for (int row = 0; row < 4; ++row)
				for (int col = 0; col < 4; ++col)
					result[col][row] = m[row][col];
			return result;
		}

		uint64_t HashMesh(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices)
		{
			uint64_t hash = 1469598103934665603ull;
			auto mix = [&hash](const void* data, size_t size) {
				const auto* bytes = static_cast<const unsigned char*>(data);
				for (size_t i = 0; i < size; ++i) {
					hash ^= bytes[i];
					hash *= 1099511628211ull;
				}
			};

			mix(vertices.data(), vertices.size() * sizeof(MeshVertex));
			mix(indices.data(), indices.size() * sizeof(uint32_t));
			return hash;
		}

		bool SameMesh(const MeshCandidate& candidate,
			const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices)
		{
			if (candidate.vertices.size() != vertices.size() ||
				candidate.indices.size() != indices.size())
				return false;

			return std::memcmp(candidate.vertices.data(), vertices.data(),
					vertices.size() * sizeof(MeshVertex)) == 0 &&
				std::memcmp(candidate.indices.data(), indices.data(),
					indices.size() * sizeof(uint32_t)) == 0;
		}

		bool BuildMeshData(const aiMesh& mesh, std::vector<MeshVertex>& vertices,
			std::vector<uint32_t>& indices)
		{
			if (mesh.mNumVertices == 0 || mesh.mNumFaces == 0)
				return false;

			const bool hasUv = mesh.mTextureCoords[0] != nullptr &&
				mesh.mNumUVComponents[0] >= 2;

			vertices.resize(mesh.mNumVertices);
			for (uint32_t i = 0; i < mesh.mNumVertices; ++i) {
				MeshVertex& vertex = vertices[i];
				vertex.position = glm::vec3(mesh.mVertices[i].x, mesh.mVertices[i].y, mesh.mVertices[i].z);
				vertex.uv = hasUv
					? glm::vec2(mesh.mTextureCoords[0][i].x, mesh.mTextureCoords[0][i].y)
					: glm::vec2(0.0f);
				vertex.normal = mesh.mNormals
					? glm::vec3(mesh.mNormals[i].x, mesh.mNormals[i].y, mesh.mNormals[i].z)
					: glm::vec3(0.0f, 1.0f, 0.0f);
			}

			indices.clear();
			indices.reserve(static_cast<size_t>(mesh.mNumFaces) * 3);
			for (uint32_t f = 0; f < mesh.mNumFaces; ++f) {
				const aiFace& face = mesh.mFaces[f];
				// Triangulate 之后不该出现非三角形;真出现了就跳过整个 mesh,
				// 不往大 buffer 里塞半份几何。
				if (face.mNumIndices != 3)
					return false;

				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}

			return !indices.empty();
		}

		class ImportContext
		{
		public:
			ImportContext(const aiScene& scene, const Ref<Shader>& shader)
				: m_Scene(scene), m_Shader(shader)
			{}

			SceneInstance Run()
			{
				SceneInstance result;
				Traverse(*m_Scene.mRootNode, glm::mat4(1.0f), result);
				return result;
			}

		private:
			void Traverse(const aiNode& node, const glm::mat4& parent, SceneInstance& result)
			{
				const glm::mat4 world = parent * ToGlm(node.mTransformation);

				for (uint32_t i = 0; i < node.mNumMeshes; ++i) {
					const unsigned int sourceIndex = node.mMeshes[i];
					if (sourceIndex >= m_Scene.mNumMeshes || m_Scene.mMeshes[sourceIndex] == nullptr)
						continue;

					const aiMesh& mesh = *m_Scene.mMeshes[sourceIndex];

					if (mesh.mNumBones > 0) {
						FS_CORE_WARN("跳过蒙皮 mesh:{}", mesh.mName.C_Str());
						continue;
					}

					const MeshId id = GetOrCreateMesh(sourceIndex, mesh);
					if (id == k_InvalidMeshId)
						continue;

					result.instances.push_back(Instance{
						.meshId      = id,
						.transform   = world,
						.color       = glm::vec4(1.0f),
						.textureSlot = 0,
						.samplerSlot = 0,
						.shader      = m_Shader });
				}

				for (uint32_t i = 0; i < node.mNumChildren; ++i) {
					if (node.mChildren[i] != nullptr)
						Traverse(*node.mChildren[i], world, result);
				}
			}

			MeshId GetOrCreateMesh(unsigned int sourceIndex, const aiMesh& mesh)
			{
				// 同一个 aiMesh 被多个节点引用:先按下标复用,不用碰数据。
				if (const auto it = m_BySource.find(sourceIndex); it != m_BySource.end())
					return it->second;

				std::vector<MeshVertex> vertices;
				std::vector<uint32_t> indices;
				if (!BuildMeshData(mesh, vertices, indices)) {
					FS_CORE_WARN("跳过不可用的 mesh:{}", mesh.mName.C_Str());
					return k_InvalidMeshId;
				}

				const uint64_t hash = HashMesh(vertices, indices);
				std::vector<size_t>& bucket = m_ByHash[hash];
				for (const size_t candidateIndex : bucket) {
					if (SameMesh(m_Candidates[candidateIndex], vertices, indices)) {
						const MeshId id = m_Candidates[candidateIndex].id;
						m_BySource.emplace(sourceIndex, id);
						return id;
					}
				}

				const MeshId id = Renderer::CreateMesh(vertices.data(),
					static_cast<uint32_t>(vertices.size()), indices.data(),
					static_cast<uint32_t>(indices.size()));
				if (id == k_InvalidMeshId)
					return k_InvalidMeshId;

				bucket.push_back(m_Candidates.size());
				m_Candidates.push_back(MeshCandidate{
					.vertices = std::move(vertices),
					.indices  = std::move(indices),
					.id       = id });
				m_BySource.emplace(sourceIndex, id);
				return id;
			}

			const aiScene& m_Scene;
			Ref<Shader>    m_Shader;

			// 只活在一次导入里:源下标 -> meshId,内容哈希 -> 候选。
			std::unordered_map<unsigned int, MeshId>        m_BySource;
			std::unordered_map<uint64_t, std::vector<size_t>> m_ByHash;
			std::vector<MeshCandidate>                      m_Candidates;
		};
	}

	SceneInstance ImportScene(const std::string& path, const Ref<Shader>& shader)
	{
		Assimp::Importer importer;

		// 点和线没有绘制路径,在拆分时直接丢掉。
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE,
			aiPrimitiveType_POINT | aiPrimitiveType_LINE);

		unsigned int flags =
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_GenSmoothNormals |
			aiProcess_SortByPType |
			aiProcess_ImproveCacheLocality;

		// glTF 的 UV 原点在图像左上;引擎贴图加载已经上下翻转(VulkanTexture.cpp:22),
		// OBJ 的 UV 惯例本来就是左下,所以只给 glTF 翻。
		const size_t dot = path.find_last_of('.');
		std::string extension = dot == std::string::npos ? std::string{} : path.substr(dot);
		for (char& c : extension)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (extension == ".gltf" || extension == ".glb")
			flags |= aiProcess_FlipUVs;

		const aiScene* scene = importer.ReadFile(path.c_str(), flags);
		if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) ||
			scene->mRootNode == nullptr) {
			FS_CORE_ERROR("导入失败:{} — {}", path, importer.GetErrorString());
			return {};
		}

		return ImportContext(*scene, shader).Run();
	}
}
