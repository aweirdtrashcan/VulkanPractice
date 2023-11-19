#pragma once

#include <DirectXMath.h>
#include <vector>

class GeometryGenerator
{
public:
	struct Vertex
	{
		Vertex() :
			Position(0.0f, 0.0f, 0.0f),
			Normal(0.0f, 0.0f, 0.0f),
			TangentU(0.0f, 0.0f, 0.0f),
			TexC(0.0f, 0.0f),
			Color(0.0f, 0.0f, 0.0f, 0.0f)
		{}
		Vertex(
			const DirectX::XMFLOAT3& p, 
			const DirectX::XMFLOAT3& n, 
			const DirectX::XMFLOAT3& t, 
			const DirectX::XMFLOAT2& uv,
			const DirectX::XMFLOAT4& c = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f)) :
			Position(p),
			Normal(n),
			TangentU(t),
			TexC(uv),
			Color(c)
		{}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v,
			float a = 0.0f, float r = 0.0f, float g = 0.0f, float b = 0.0f) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			TangentU(tx, ty, tz),
			TexC(u, v),
			Color(a, r, g, b)
		{}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT2 TexC;
		DirectX::XMFLOAT4 Color;
	};

	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices32;

		std::vector<uint16_t>& GetIndices16() {
			if (mIndices16.empty()) {
				mIndices16.resize(Indices32.size());
				for (size_t i = 0; i < Indices32.size(); i++) {
					mIndices16[i] = static_cast<uint16_t>(Indices32[i]);
				}
			}

			return mIndices16;
		}

	private:
		std::vector<uint16_t> mIndices16;
	};

	///<summary>
	/// Creates a box centered at the origin with the given dimensions, where each
    /// face has m rows and n columns of vertices.
	///</summary>
    MeshData CreateBox(float width, float height, float depth, uint32_t numSubdivisions);

	///<summary>
	/// Creates a sphere centered at the origin with the given radius.  The
	/// slices and stacks parameters control the degree of tessellation.
	///</summary>
    MeshData CreateSphere(float radius, uint32_t sliceCount, uint32_t stackCount);

	///<summary>
	/// Creates a geosphere centered at the origin with the given radius.  The
	/// depth controls the level of tessellation.
	///</summary>
    MeshData CreateGeosphere(float radius, uint32_t numSubdivisions);

	///<summary>
	/// Creates a cylinder parallel to the y-axis, and centered about the origin.  
	/// The bottom and top radius can vary to form various cone shapes rather than true
	// cylinders.  The slices and stacks parameters control the degree of tessellation.
	///</summary>
    MeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount);

	///<summary>
	/// Creates an mxn grid in the xz-plane with m rows and n columns, centered
	/// at the origin with the specified width and depth.
	///</summary>
    MeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);

	///<summary>
	/// Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
	///</summary>
    MeshData CreateQuad(float x, float y, float w, float h, float depth);

private:
	void BuildCylinderTopCap(float bottomRadius, float topRadius, 
		float height, uint32_t sliceCount, 
		uint32_t stackCount, GeometryGenerator::MeshData& meshData);

	void BuildCylinderBottomCap(float bottomRadius, float topRadius,
		float height, uint32_t sliceCount,
		uint32_t stackCount, GeometryGenerator::MeshData& meshData);

	void Subdivide(MeshData& meshData) const;

	Vertex MidPoint(const Vertex& v0, const Vertex& v1) const;
};

