// WaterSurface.cpp

#include "WaterSurface.h"
#include <ppl.h>
#include <algorithm>
#include <vector>
#include <cassert>

using namespace DirectX;

WaterSurface::WaterSurface(int row, int col, float ds, float dt, float v, float gamma)
{
	mRowCount = row;
	mColCount = col;

	mVertexCount = row * col;
	mTriangleCount = (row - 1) * (col - 1) * 2;

	mDt = dt;
	mDs = ds;

	float f1 = gamma * dt + 2.0f;
	float f2 = gamma * dt - 2.0f;
	float f3 = (v * dt) * (v * dt) / (ds * ds);
	mc1 = f2 / f1;
	mc2 = 4.0f * (1.0f - 2.0f * f3) / f1;
	mc3 = 2.0f * f3 / f1;

	mPrevVertices.resize(row * col);
	mCurrVertices.resize(row * col);
	mNormals.resize(row * col);
	mTangentX.resize(row * col);

	// set up water surface geometry as a 2D lattice in the host memroy
	float halfWidth = (col - 1) * ds * 0.5f;
	float halfDepth = (row - 1) * ds * 0.5f;

	for (int i = 0; i < row; ++i)
	{
		float z = halfDepth - (float)i * ds;
		for (int j = 0; j < col; ++j)
		{
			float x = -halfWidth + (float)j * ds;

			mPrevVertices[i * col + j] = XMFLOAT3(x, 0.0f, z);			// mPrevVertices, mCurrVertices being updated according to the given difference equation
			mCurrVertices[i * col + j] = XMFLOAT3(x, 0.0f, z);			// they continuously switch each other to emulate the propagation of waves.
			mNormals[i * col + j] = XMFLOAT3(0.0f, 1.0f, 0.0f);			// normal vectors attached to vertices situated at grid point in the 2D lattice.
			mTangentX[i * col + j] = XMFLOAT3(1.0f, 0.0f, 0.0f);		// unit vector aligned in x-coord direction, initially
		}
	}
}

WaterSurface::~WaterSurface()
{

}

int WaterSurface::GetRowCount() const
{
	return mRowCount;
}

int WaterSurface::GetColumnCount() const
{
	return mColCount;
}

int WaterSurface::GetVertexCount() const
{
	return mVertexCount;
}

int WaterSurface::GetTriangleCount() const
{
	return mTriangleCount;
}

float WaterSurface::GetsurfWidth() const
{
	return mColCount * mDs;
}

float WaterSurface::GetsurfDepth() const
{
	return mRowCount * mDs;
}

void WaterSurface::UpdateModelEquation(float dt)
{
	static float t = 0;
	t += dt;

	// update the equation in every fixed time step
	if (t >= mDt)
	{
		// use parallel_for and lambda function for faster update.
		concurrency::parallel_for(1, mRowCount - 1, [this](int i)
			{
				for (int j = 1; j < mColCount - 1; ++j)
				{
					mPrevVertices[i * mColCount + j].y =
						mc1 * mPrevVertices[i * mColCount + j].y +
						mc2 * mCurrVertices[i * mColCount + j].y +
						mc3 * (mCurrVertices[(i + 1) * mColCount + j].y +
							mCurrVertices[(i - 1) * mColCount + j].y +
							mCurrVertices[i * mColCount + j + 1].y + 
							mCurrVertices[i * mColCount + j - 1].y);
				}
			});

		std::swap(mPrevVertices, mCurrVertices);

		t = 0.0f;	// reset time for the next update

		// update normal vectors 
		concurrency::parallel_for(1, mRowCount - 1, [this](int i)
			{
				for (int j = 1; j < mColCount - 1; ++j)
				{
					float left = mCurrVertices[i * mColCount + j - 1].y;
					float right = mCurrVertices[i * mColCount + j + 1].y;
					float top = mCurrVertices[(i - 1) * mColCount + j].y;
					float bottom = mCurrVertices[(i + 1) * mColCount + j].y;
					// set normal vector at a grid (i, j) with neighboring normal's heights
					mNormals[i * mColCount + j].x = left - right;
					mNormals[i * mColCount + j].y = 2.0f * mDs;		// an interval between left and right neighboring vertices
					mNormals[i * mColCount + j].z = bottom - top;
					// normalize it
					XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&mNormals[i * mColCount + j]));
					XMStoreFloat3(&mNormals[i * mColCount + j], n);

					// set tanget to x
					mTangentX[i * mColCount + j] = XMFLOAT3(2.0f * mDs, right - left, 0.0f);
					XMVECTOR T = XMVector3Normalize(XMLoadFloat3(&mTangentX[i * mColCount + j]));
					// normalize it
					XMStoreFloat3(&mTangentX[i * mColCount + j], T);
				}
			});
	}
}

void WaterSurface::AddFluctuationsAt(int i, int j, float intensity)
{
	// no excitation on boundary vectors
	assert(i > 1 && i < mRowCount - 2);
	assert(j > 1 && j < mColCount - 2);

	mCurrVertices[i * mColCount + j].y += intensity;
	mCurrVertices[i * mColCount + j + 1].y += intensity * 0.5f;
	mCurrVertices[i * mColCount + j - 1].y += intensity * 0.5f;
	mCurrVertices[(i + 1) * mColCount + j].y += intensity * 0.5f;
	mCurrVertices[(i - 1) * mColCount + j].y += intensity * 0.5f;
}





