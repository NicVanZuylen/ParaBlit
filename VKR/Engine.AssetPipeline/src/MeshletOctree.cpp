#include "MeshletOctree.h"

namespace AssetPipeline
{
	void MeshletOctree::OutputIndexBuffer(MeshEncoder::IndexBuffer& indices)
	{
		m_root->OutputIndices(indices);
	}

	glm::vec3 Triangle::GetCenter(const MeshEncoder::VertexBuffer& vertices) const
	{
		glm::vec3 c = vertices[m_indices[0]].m_position + vertices[m_indices[1]].m_position + vertices[m_indices[2]].m_position;
		c /= 3.0f;
		return c;
	}

	MeshletOctree::Node::~Node()
	{
		for (Node*& child : m_children)
		{
			if(child != nullptr)
				delete child;
		}
	}

	void MeshletOctree::Node::AddTriangle(const Triangle& triangle)
	{
		if (m_children[0] != nullptr)
		{
			glm::vec3 triCenter = triangle.GetCenter(*m_vertices);
			assert(m_bounds.RoughlyContains(triCenter));

			bool foundBucket = false;
			for (Node*& child : m_children)
			{
				if (child->m_bounds.RoughlyContains(triCenter))
				{
					child->AddTriangle(triangle);
					foundBucket = true;
					break;
				}
			}

			assert(foundBucket == true);
		}
		else
		{
			assert(m_bounds.RoughlyContains(triangle.GetCenter(*m_vertices)));
			m_triangles.PushBack(triangle);

			if (m_triangles.Count() > MeshletSize)
			{
				Split();
			}
		}
	}

	void MeshletOctree::Node::OutputIndices(MeshEncoder::IndexBuffer& indices)
	{
		if (m_triangles.Count() > 0)
		{
			SortByProximity(m_bounds.m_origin);

			uint32_t oldCount = indices.Count();
			indices.SetCount(indices.Count() + (m_triangles.Count() * 3));
			memcpy(&indices[oldCount], m_triangles.Data(), sizeof(Triangle) * m_triangles.Count());
		}
		else
		{
			if (m_children[0] == nullptr)
				return;

			const auto childSort = [](const Node* a, const Node* b)
			{
				return a->m_triangles.Count() > b->m_triangles.Count();
			};
			std::sort(m_children, &m_children[8], childSort);

			for (Node* child : m_children)
				child->OutputIndices(indices);
		}
	}

	void MeshletOctree::Node::SortByProximity(glm::vec3 point)
	{
		auto proximitySort = [=](const Triangle& a, const Triangle& b) -> bool
		{
			float aDist = glm::distance(a.GetCenter(*m_vertices), point);
			float bDist = glm::distance(b.GetCenter(*m_vertices), point);
			return aDist < bDist;
		};

		std::sort(m_triangles.begin(), m_triangles.end(), proximitySort);
	}

	void MeshletOctree::Node::Split()
	{
		Bounds childBounds = m_bounds;
		childBounds.m_extents /= 2.0f;

		// Bottom front left.
		m_children[0] = new Node(m_vertices, childBounds);

		// Bottom front right.
		childBounds.m_origin.x = childBounds.MaxX();
		m_children[1] = new Node(m_vertices, childBounds);

		// Bottom back right.
		childBounds.m_origin.z = childBounds.MaxZ();
		m_children[2] = new Node(m_vertices, childBounds);

		// Bottom back left.
		childBounds.m_origin.x = m_bounds.m_origin.x;
		m_children[3] = new Node(m_vertices, childBounds);

		// Top front left.
		childBounds.m_origin.z = m_bounds.m_origin.z;
		childBounds.m_origin.y = childBounds.MaxY();
		m_children[4] = new Node(m_vertices, childBounds);

		// Top front right.
		childBounds.m_origin.x = childBounds.MaxX();
		m_children[5] = new Node(m_vertices, childBounds);

		// Top back right.
		childBounds.m_origin.z = childBounds.MaxZ();
		m_children[6] = new Node(m_vertices, childBounds);

		// Top back left.
		childBounds.m_origin.x = m_bounds.m_origin.x;
		m_children[7] = new Node(m_vertices, childBounds);

		for (const Triangle& tri : m_triangles)
		{
			glm::vec3 triCenter = tri.GetCenter(*m_vertices);

			for (Node*& child : m_children)
			{
				if (child->m_bounds.RoughlyContains(triCenter))
				{
					child->AddTriangle(tri);
					break;
				}
			}
		}
		m_triangles.Clear();

		for (Node*& child : m_children)
		{
			child->SortByProximity(m_bounds.Center());
		}
	}

	MeshletOctree::~MeshletOctree()
	{
		delete m_root;
		m_root = nullptr;
	}
};