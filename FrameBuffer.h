// Frame Buffer that stores resources required for GPU to render a scene

#pragma once

#include "Helpers/d3dUtil.h"
#include "Helpers/MathHelper.h"
#include "Helpers/UploadBuffer.h"

// object constant, supposed to paired to object cbuffer in hlsl source
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

// common constant, supposed to paired to common cbuffer in hlsl source
struct CommonConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 CameraPosW = { 0.0f, 0.0f, 0.0f };
	float cbCommonPad0 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.1f, 0.1f, 0.1f, 1.0f };

	DirectX::XMFLOAT4 FogEffectColor = { 0.9f, 0.9f, 0.9f, 1.0f };

	Light Lights[MaxLights];
};


struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct FrameBuffer
{
	FrameBuffer(ID3D12Device* device, UINT commonCount, UINT objectCount, UINT materialCount, UINT waterVertexCount);
	FrameBuffer(const FrameBuffer& rhs) = delete;
	FrameBuffer& operator=(const FrameBuffer& rhs) = delete;
	~FrameBuffer();

	// each frame buffer must have a independent command list allocator to properly add commands onto the command list: ID3D12CommandList
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// using template wrapper class UploadBuffer<T> to manage rendering resources both on system memory and GPU memory
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<CommonConstants>> CommonCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

	// store water surface related resources since their vertices dynamically changes frame per frame.
	std::unique_ptr<UploadBuffer<Vertex>> WaterSurfaceVB = nullptr;

	UINT64 Fence = 0;
};

