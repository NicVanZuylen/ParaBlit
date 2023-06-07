#include "Bounds.h"
#include "Encode/MeshEncoder.h"

#include <CLib/Vector.h>

namespace AssetPipeline
{
	struct Triangle
	{
		glm::vec3 GetCenter(const MeshEncoder::VertexBuffer& vertices) const;

		glm::uvec3 m_indices;
	};

	class MeshletOctree
	{
	public:

		static constexpr uint32_t MeshletSize = 32;
		static constexpr uint32_t MeshletIndexSize = 32 * 3;

		MeshletOctree(const Bounds& rootBounds, MeshEncoder::VertexBuffer* vertices)
		{
			assert(vertices != nullptr);
			m_vertices = vertices;

			m_root = new Node(m_vertices, rootBounds);
		}

		~MeshletOctree();

		void AddTriangle(const Triangle& triangle) { m_root->AddTriangle(triangle); }

		void OutputIndexBuffer(MeshEncoder::IndexBuffer& indices);

	private:

		class Node
		{
		public:

			Node(MeshEncoder::VertexBuffer* vertices, const Bounds& bounds) 
			{
				m_vertices = vertices;
				m_bounds = bounds;
			};

			~Node();

			void AddTriangle(const Triangle& triangle);

			void OutputIndices(MeshEncoder::IndexBuffer& indices);

			void SortByProximity(glm::vec3 point);

		private:

			void Split();

			Bounds m_bounds;
			CLib::Vector<Triangle, MeshletSize + 1> m_triangles;
			Node* m_children[8]{};
			MeshEncoder::VertexBuffer* m_vertices = nullptr;
		};

		Node* m_root = nullptr;
		MeshEncoder::VertexBuffer* m_vertices = nullptr;
	};
};