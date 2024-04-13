#pragma once
// it simulates the water surface mesh fluctuating about vertical direction 
// when point-like disturbances are applied to it.
// the wave spread model is followed from a 2D heat conduction differential equation. 


#include <vector>
#include <DirectXMath.h>

class WaterSurface
{
public:
	WaterSurface(int row, int col, float ds, float dt, float v, float gamma);
	WaterSurface(const WaterSurface& rhs) = delete;
	WaterSurface& operator=(const WaterSurface& rhs) = delete;
	~WaterSurface();

	int GetRowCount() const;
	int GetColumnCount() const;
	int GetVertexCount() const;
	int GetTriangleCount() const;
	float GetsurfWidth() const;
	float GetsurfDepth() const;

	const DirectX::XMFLOAT3& Position(int i) const
	{
		return mCurrVertices[i];
	}
	
	const DirectX::XMFLOAT3& Normal(int i) const
	{
		return mNormals[i];
	}

	const DirectX::XMFLOAT3& TangetX(int i) const
	{
		return mTangentX[i];
	}

	void UpdateModelEquation(float dt);
	void AddFluctuationsAt(int i, int j, float intensity);

private:
	int mRowCount = 0;
	int mColCount = 0;

	int mVertexCount = 0;
	int mTriangleCount = 0;

	// simulation constants
	float mc1 = 0.0f;
	float mc2 = 0.0f;
	float mc3 = 0.0f;

	float mDs = 0.0f;	// a unit spatial step in both horizontal and vertical directions
	float mDt = 0.0f;	// a unit temporal step between which the simulation updates.

	std::vector<DirectX::XMFLOAT3> mCurrVertices;
	std::vector<DirectX::XMFLOAT3> mPrevVertices;
	std::vector<DirectX::XMFLOAT3> mNormals;
	std::vector<DirectX::XMFLOAT3> mTangentX;
};


