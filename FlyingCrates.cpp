// Flying Crates Prototype Game Demo v1.1
// using Microsoft WIN32 API & Microsoft DirectX 12
// Programmed by H.H. Yoo

#include "FlyingCrates.h"
#include "framework.h"
#include "Resource.h"

#include "Helpers/d3dApp.h"
#include "Helpers/MathHelper.h"
#include "Helpers/UploadBuffer.h"
#include "Helpers/GeometryGenerator.h"
#include "FrameBuffer.h"
#include "WaterSurface.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameBuffers = 3;

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// for static object. static object doesn't need continuous buffer update 
	int numFrameBufferFilled = gNumFrameBuffers;	

	//  object constant buffer index
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	bool isItemStatic = false;
	bool isItemActivated;

	// ID3D12GraphicsCommandList::DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	Sky,
	Player,
	Shell,
	Enemy,
	Count		// the total count of elements
};


struct WPosition			// for storing position vector out of a world matrix
{
	float x;
	float y;
	float z;
};

struct EnergyShell
{
	RenderItem* rpShell[5];					// RenderItem pointer Shell
	float speed;
};


struct Player
{
	RenderItem* objPlayer;
	EnergyShell myShell;
	float playerScale;
	WPosition plPosition;
	size_t killCount;
};

struct Enemy
{
	RenderItem* rpEnemy[5];					// RenderItem pointer Enemy
	float speed[5];
	XMFLOAT4X4 refPosition;
};

bool CloseEnough(WPosition pt1, WPosition pt2, float dist);

class FlyingCrates : public D3DApp
{
public:
	FlyingCrates(HINSTANCE hInstance);
	FlyingCrates(const FlyingCrates& rhs) = delete;
	FlyingCrates& operator=(const FlyingCrates& rhs) = delete;
	~FlyingCrates();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateTextures(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateCommonCB(const GameTimer& gt);
	void UpdateWaterSurface(const GameTimer& gt);
	void UpdateEnemies(const GameTimer& gt);
	void WriteCaption();

	void PrepareTextures();
	void SetRootSignature();
	void SetDescriptorHeaps();
	void SetShadersAndInputLayout();
	void SetTerrainGeometry();
	void SetWaterGeometry();
	void SetFiguresGeometry();
	void CollisionProcessing(RenderItem* player, RenderItem** shells, RenderItem** enemies, const int& shellCount, const int& enemyCount);
	void SetPSOs();
	void SetFrameBuffers();
	void SetMaterials();
	void SetRenderingItems();

	void DrawRenderingItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);
	void DrawGroupItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems);

	array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();		// static samplers for wrapping textures on objects

	float GetHillsHeight(float x, float z) const;
	XMFLOAT3 GetHillsNormal(float x, float z) const;

private:
	vector<unique_ptr<FrameBuffer>> mFrameBuffers;	// to maintain FrameBuffers in a circular array
	FrameBuffer* mCurrFrameBuffer = nullptr;
	int mCurrFrameBufferIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;			// query the device the size of constant buffer view, shader resource view and store them.

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	unordered_map<string, unique_ptr<MeshGeometry>> mGeometries;				// categorize different mesh geometries by name(std::string).
	unordered_map<string, unique_ptr<Material>> mMaterials;						// categorize different materials by name.
	unordered_map<string, unique_ptr<Texture>> mTextures;						// categorize different textures by name.
	unordered_map<string, ComPtr<ID3DBlob>> mShaders;							// categorize compiled shader source by name.
	unordered_map<string, ComPtr<ID3D12PipelineState>> mPSOs;					// categorize differently defined pipeline state objects by name.

	vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;								// layout of data supplied to IA(Input Assembler) of the rendering pipeline.

	RenderItem* mWaterRitem = nullptr;											// for applying vertices of water object dynamically

	// List of all the rendering items.
	vector<unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	unique_ptr<WaterSurface> mWaterSurface;

	CommonConstants mCommonCB;
	
	UINT mSkyCubeTexHeapIndex = 0;

	Player mPlayer;
	Enemy mEnemy;

	float mLastFireTime = 0.0f;
	float maneuverSpeed = 50.0f;

	XMFLOAT3 mCameraPos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 0.0f;
	float mPhi = 0.0f;
	float mRadius = 0.0f;

	float mPeakHeight = 0.05f;

	POINT mLastMousePos;
};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		FlyingCrates thisApp(hInstance);
		if (!thisApp.Initialize())
		{
			return 0;
		}
		return thisApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed...", MB_OK);
		return 0;
	}
}

FlyingCrates::FlyingCrates(HINSTANCE hInstance) : D3DApp(hInstance)
{
	this->mRadius = 350.0f;
	this->mPhi = MathHelper::Pi / 2.8f;
	this->mTheta = -XM_PIDIV2;

	// set initial position of the player appeared in the scene.
	mPlayer.playerScale = 15.0f;
	mPlayer.plPosition.x = 0.0f;
	mPlayer.plPosition.y = 50.0f;
	mPlayer.plPosition.z = -200.0f;

	mPlayer.myShell.speed = 250.0f;
	mPlayer.killCount = 0;

	for (int i = 0; i < 5; ++i)
	{
		mEnemy.speed[i] = 30.0f;
	}
	mEnemy.refPosition = MathHelper::Identity4x4();

	mLastFireTime = D3DApp::mTimer.TotalTime();
}

FlyingCrates::~FlyingCrates()
{
	if (md3dDevice != nullptr)
	{
		FlushCommandQueue();
	}
}

bool FlyingCrates::Initialize()
{
	if (!D3DApp::Initialize())
	{
		return false;
	}

	// Reset the command list to prep. for initialization commands.
	ThrowIfFailed(mCommandList->Reset(D3DApp::mDirectCmdListAlloc.Get(), nullptr));

	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mWaterSurface = make_unique<WaterSurface>(200, 200, 2.0f, 0.03f, 5.0f, 0.1f);

	PrepareTextures();
	SetRootSignature();
	SetDescriptorHeaps();					// set descriptor heaps inside which shader resources descriptors are recorded.
	SetShadersAndInputLayout();				// compile shaders and set input layout to IA(Input Assembler)
	SetTerrainGeometry();					// set mesh geometries for the terrain.
	SetWaterGeometry();						// set water mesh geometry
	SetFiguresGeometry();
	SetMaterials();
	SetRenderingItems();					// prepare rendering items: their geometries, material properties, and textures are set.
	SetFrameBuffers();
	SetPSOs();								// set rendering pipeline state objects.

	// execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wait until initialization is done.
	FlushCommandQueue();

	return true;
}

void FlyingCrates::OnResize()
{
	D3DApp::OnResize();
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void FlyingCrates::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);
	UpdateEnemies(gt);
	CollisionProcessing(mPlayer.objPlayer, mPlayer.myShell.rpShell, mEnemy.rpEnemy, 5, 5);
	WriteCaption();

	// cycle through the circular frame buffer array.
	mCurrFrameBufferIndex = (mCurrFrameBufferIndex + 1) % gNumFrameBuffers;
	mCurrFrameBuffer = mFrameBuffers[mCurrFrameBufferIndex].get();

	// in case the GPU has not finished processing all the commands of the current frame buffer,
	// wait until the GPU has completed all commands up to this fence point. (using event object)
	if (mCurrFrameBuffer->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameBuffer->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameBuffer->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateTextures(gt);		// it implements visual effect of flowing water surface.
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateCommonCB(gt);
	UpdateWaterSurface(gt);
}

void FlyingCrates::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameBuffer->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));		// set pipeline state object mPSOs["opaque"] for a default PSO.

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mCommonCB.FogEffectColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto commonCB = mCurrFrameBuffer->CommonCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, commonCB->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mSkyCubeTexHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);

	// draw a opaque object : terrain
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// draw a transparent object : water surface
	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	// draw a player object : a crate
	mCommandList->SetPipelineState(mPSOs["player"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Player]);

	// draw shells getting fired from the player
	mCommandList->SetPipelineState(mPSOs["shell"].Get());
	DrawGroupItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Shell]);

	// draw enemy cubes
	mCommandList->SetPipelineState(mPSOs["enemy"].Get());
	DrawGroupItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Enemy]);

	// draw a far-sighted sky
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// put the command list to the queue for execution
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// advance the fence value to mark commands up to this fence point.
	mCurrFrameBuffer->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void FlyingCrates::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void FlyingCrates::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void FlyingCrates::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		//float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta += dx;
		//mPhi += dy;

		// Restrict the angle mPhi
		//mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

		// update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 150.0f, 350.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void FlyingCrates::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	XMFLOAT4X4 checkMat;
	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
	{
		XMMATRIX worldRes = XMLoadFloat4x4(&mPlayer.objPlayer->World);
		XMStoreFloat4x4(&checkMat, worldRes);
		if (checkMat._41 > -120.0f)
		{
			worldRes = worldRes * XMMatrixTranslation(-maneuverSpeed * dt, 0.0f, 0.0f);
			mPlayer.plPosition.x = checkMat._41 - maneuverSpeed * dt;			// player's position update
		}
		XMStoreFloat4x4(&mPlayer.objPlayer->World, worldRes);
	}

	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
	{
		XMMATRIX worldRes = XMLoadFloat4x4(&mPlayer.objPlayer->World);
		XMStoreFloat4x4(&checkMat, worldRes);
		if (checkMat._41 < 120.0f)
		{
			worldRes = worldRes * XMMatrixTranslation(+maneuverSpeed * dt, 0.0f, 0.0f);
			mPlayer.plPosition.x = checkMat._41 + maneuverSpeed * dt;			// player's position update
		}
		XMStoreFloat4x4(&mPlayer.objPlayer->World, worldRes);
	}

	if (GetAsyncKeyState(VK_UP) & 0x8000)
	{
		XMMATRIX worldRes = XMLoadFloat4x4(&mPlayer.objPlayer->World);
		XMStoreFloat4x4(&checkMat, worldRes);
		if (checkMat._43 < 150.0f)
		{
			worldRes = worldRes * XMMatrixTranslation(0.0f, 0.0f, +maneuverSpeed * dt);
			mPlayer.plPosition.z = checkMat._43 + maneuverSpeed * dt;			// player's position update
		}
		XMStoreFloat4x4(&mPlayer.objPlayer->World, worldRes);
	}

	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
	{
		XMMATRIX worldRes = XMLoadFloat4x4(&mPlayer.objPlayer->World);
		XMStoreFloat4x4(&checkMat, worldRes);
		if (checkMat._43 > -210.0f)
		{
			worldRes = worldRes * XMMatrixTranslation(0.0f, 0.0f, -maneuverSpeed * dt);
			mPlayer.plPosition.z = checkMat._43 - maneuverSpeed * dt;			// player's position update
		}
		XMStoreFloat4x4(&mPlayer.objPlayer->World, worldRes);
	}

	// renew shells' world matrix with player's crate world matrix
	for (int i = 0; i < 5; ++i)
	{
		// if a shell is out of range, deactivate it.
		float distance = mPlayer.myShell.rpShell[i]->World._43;
		if (distance >= 200.0f)
		{
			mPlayer.myShell.rpShell[i]->isItemActivated = false;
		}
		if (mPlayer.myShell.rpShell[i]->isItemActivated == false)
		{
			XMMATRIX worldRes = XMLoadFloat4x4(&mPlayer.myShell.rpShell[i]->World);
			worldRes = XMMatrixTranslation(mPlayer.plPosition.x, 50.0f, mPlayer.plPosition.z);
			XMStoreFloat4x4(&mPlayer.myShell.rpShell[i]->World, worldRes);
		}
	}

	if (GetAsyncKeyState(VK_SPACE) & 0x8000)
	{
		float temp = gt.TotalTime();
		if (temp - mLastFireTime >= 0.3f)
		{
			mLastFireTime = temp;

			for (int i = 0; i < 5; ++i)
			{
				if (mPlayer.myShell.rpShell[i]->isItemActivated == false)
				{
					mPlayer.myShell.rpShell[i]->isItemActivated = true;
					break;
				}
			}
		}
	}

	// flying shell update
	for (int i = 0; i < 5; ++i)
	{
		if (mPlayer.myShell.rpShell[i]->isItemActivated == true)
		{
			XMMATRIX worldShell = XMLoadFloat4x4(&mPlayer.myShell.rpShell[i]->World);
			worldShell = worldShell * XMMatrixTranslation(0.0f, 0.0f, mPlayer.myShell.speed * dt);
			XMStoreFloat4x4(&mPlayer.myShell.rpShell[i]->World, worldShell);
		}
	}
}

void FlyingCrates::UpdateEnemies(const GameTimer& gt)
{
	// update enemies position.
	const float dt = gt.DeltaTime();

	for (int i = 0; i < 5; ++i)
	{
		if (mEnemy.rpEnemy[i]->isItemActivated == false)
		{
			float xFluc = MathHelper::RandF(-20.0f, 20.0f);
			float zFluc = MathHelper::RandF(-20.0f, 20.0f);
			float spd = MathHelper::RandF(+15.0f, 40.0f);
			XMMATRIX setupWorld = XMLoadFloat4x4(&mEnemy.refPosition);
			XMMATRIX worldTemp = XMLoadFloat4x4(&mEnemy.rpEnemy[i]->World);
			worldTemp = setupWorld * XMMatrixScaling(15.0f, 15.0f, 15.0f) *
				XMMatrixTranslation(-100.0f + (float)i * 50.0f, 50.0f, 300.0f) * XMMatrixTranslation(xFluc, 0.0f, zFluc);
			XMStoreFloat4x4(&mEnemy.rpEnemy[i]->World, worldTemp);
			mEnemy.speed[i] = spd;
			mEnemy.rpEnemy[i]->isItemActivated = true;
		}
	}

	for (int i = 0; i < 5; ++i)
	{
		XMFLOAT4X4 worldTemp = mEnemy.rpEnemy[i]->World;
		if (mEnemy.rpEnemy[i]->isItemActivated == true && worldTemp._43 < -230.0f)
		{
			mEnemy.rpEnemy[i]->isItemActivated = false;
		}
		else if (mEnemy.rpEnemy[i]->isItemActivated == true && worldTemp._43 >= -230.0f)
		{
			XMMATRIX worldRes = XMLoadFloat4x4(&mEnemy.rpEnemy[i]->World);
			worldRes = worldRes * XMMatrixTranslation(0.0f, 0.0f, -mEnemy.speed[i] * dt);
			XMStoreFloat4x4(&mEnemy.rpEnemy[i]->World, worldRes);
		}
	}
}

void FlyingCrates::UpdateCamera(const GameTimer& gt)
{
	mCameraPos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mCameraPos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mCameraPos.y = mRadius * cosf(mPhi);

	// set the view matrix.
	XMVECTOR pos = XMVectorSet(mCameraPos.x, mCameraPos.y, mCameraPos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void FlyingCrates::AnimateTextures(const GameTimer& gt)
{
	// scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.1f * gt.DeltaTime();

	if (tu >= 1.0f)
	{
		tu -= 1.0f;
	}
	if (tv >= 1.0f)
	{
		tv -= 1.0f;
	}
	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	waterMat->NumFramesDirty = gNumFrameBuffers;
}

void FlyingCrates::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameBuffer->ObjectCB.get();

	for (auto& elem : mAllRitems)
	{
		if (elem->numFrameBufferFilled > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&elem->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&elem->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(elem->ObjCBIndex, objConstants);

			if (elem->isItemStatic == true)
			{
				elem->numFrameBufferFilled--;
			}
		}
	}
}

void FlyingCrates::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameBuffer->MaterialCB.get();
	for (auto& elem : mMaterials)
	{
		Material* mat = elem.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
			mat->NumFramesDirty--;
		}
	}
}

void FlyingCrates::UpdateCommonCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mCommonCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mCommonCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mCommonCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mCommonCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mCommonCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mCommonCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mCommonCB.CameraPosW = mCameraPos;

	mCommonCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mCommonCB.InvRenderTargetSize = XMFLOAT2(1.0f / (float)mClientWidth, 1.0f / (float)mClientHeight);
	mCommonCB.NearZ = 1.0f;
	mCommonCB.FarZ = 1000.0f;
	mCommonCB.TotalTime = gt.TotalTime();
	mCommonCB.DeltaTime = gt.DeltaTime();
	mCommonCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };
	mCommonCB.Lights[0].Direction = { 0.57735f, -0.77735f, 0.57735f };
	mCommonCB.Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
	mCommonCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mCommonCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mCommonCB.Lights[2].Direction = { 0.5f, -0.707f, -0.707f };
	mCommonCB.Lights[2].Strength = { 0.25f, 0.25f, 0.25f };

	auto currCommonCB = mCurrFrameBuffer->CommonCB.get();
	currCommonCB->CopyData(0, mCommonCB);
}

void FlyingCrates::UpdateWaterSurface(const GameTimer& gt)
{
	// random wave is generated in every quarter second.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.10f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaterSurface->GetRowCount() - 5);
		int j = MathHelper::Rand(4, mWaterSurface->GetColumnCount() - 5);
		float r = MathHelper::RandF(0.3f, 1.0f);

		mWaterSurface->AddFluctuationsAt(i, j, r);
	}

	// update the wave equation,
	mWaterSurface->UpdateModelEquation(gt.DeltaTime());

	// upload the newly calculated vertices to the vertex buffer
	auto currWaterVB = mCurrFrameBuffer->WaterSurfaceVB.get();
	for (int i = 0; i < mWaterSurface->GetVertexCount(); ++i)
	{
		Vertex v;
		
		v.Pos = mWaterSurface->Position(i);
		v.Normal = mWaterSurface->Normal(i);

		// derive tex-coord from position by mapping [-w/2, w/2] ->[0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaterSurface->GetsurfWidth();
		v.TexC.y = 0.5f - v.Pos.z / mWaterSurface->GetsurfDepth();

		currWaterVB->CopyData(i, v);
	}
	mWaterRitem->Geo->VertexBufferGPU = currWaterVB->Resource();
}

void FlyingCrates::DrawRenderingItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameBuffer->ObjectCB->Resource();
	auto matCB = mCurrFrameBuffer->MaterialCB->Resource();

	// for each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		RenderItem* ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		texHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, texHandle);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void FlyingCrates::DrawGroupItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameBuffer->ObjectCB->Resource();
	auto matCB = mCurrFrameBuffer->MaterialCB->Resource();

	// for each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		RenderItem* ri = ritems[i];

		if (ri->isItemActivated == true)
		{
			cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
			cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
			cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

			CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			texHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
			
			D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
			D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

			cmdList->SetGraphicsRootDescriptorTable(0, texHandle);
			cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
			cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

			cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
		}
	}
}

void FlyingCrates::CollisionProcessing(RenderItem* player, RenderItem** shells, RenderItem** enemies, const int& shellCount, const int& enemyCount)
{
	XMFLOAT4X4 pl1 = player->World;

	// check collision between player and enemies.
	WPosition wPos;
	wPos.x = pl1._41;
	wPos.y = pl1._42;
	wPos.z = pl1._43;

	for (int i = 0; i < enemyCount; ++i)
	{
		WPosition currEnemyPos;
		XMFLOAT4X4 enWorld = enemies[i]->World;
		currEnemyPos.x = enWorld._41;
		currEnemyPos.y = enWorld._42;
		currEnemyPos.z = enWorld._43;

		if (CloseEnough(wPos, currEnemyPos, 15.0f))
		{
			enemies[i]->isItemActivated = false;
			mPlayer.killCount++;
		}
	}

	// check collision between shells that the player fires and incoming enemies.
	for (int i = 0; i < enemyCount; ++i)
	{
		for (int j = 0; j < shellCount; ++j)
		{
			WPosition currEnemyPos;
			WPosition currShellPos;
			XMFLOAT4X4 enWorld = enemies[i]->World;
			XMFLOAT4X4 shWorld = shells[j]->World;

			currEnemyPos.x = enWorld._41;
			currEnemyPos.y = enWorld._42;
			currEnemyPos.z = enWorld._43;

			currShellPos.x = shWorld._41;
			currShellPos.y = shWorld._42;
			currShellPos.z = shWorld._43;

			if (CloseEnough(currEnemyPos, currShellPos, 10.0f))
			{
				enemies[i]->isItemActivated = false;
				shells[j]->isItemActivated = false;
				mPlayer.killCount++;
			}
		}
	}
}

void FlyingCrates::WriteCaption()
{
	wostringstream outStr;
	outStr.precision(6);
	outStr << mPlayer.killCount << L" enemies are destroyed so far.";

	D3DApp::mMainWndCaption = outStr.str();
}

// ---------- preparatory methods ----------

void FlyingCrates::PrepareTextures()
{
	auto stoneTex = make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		stoneTex->Filename.c_str(), stoneTex->Resource, stoneTex->UploadHeap));

	auto waterTex = make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"Textures/water3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		waterTex->Filename.c_str(), waterTex->Resource, waterTex->UploadHeap));

	auto myCubeTex = make_unique<Texture>();
	myCubeTex->Name = "myCubeTex";
	myCubeTex->Filename = L"Textures/woodcrate1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		myCubeTex->Filename.c_str(), myCubeTex->Resource, myCubeTex->UploadHeap));

	auto enemyCubeTex = make_unique<Texture>();
	enemyCubeTex->Name = "enemyCubeTex";
	enemyCubeTex->Filename = L"Textures/checkboard.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		enemyCubeTex->Filename.c_str(), enemyCubeTex->Resource, enemyCubeTex->UploadHeap));

	auto myShellTex = make_unique<Texture>();
	myShellTex->Name = "myShellTex";
	myShellTex->Filename = L"Textures/energyShell.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		myShellTex->Filename.c_str(), myShellTex->Resource, myShellTex->UploadHeap));

	auto skyCubeTex = make_unique<Texture>();
	skyCubeTex->Name = "skyTex";
	skyCubeTex->Filename = L"Textures/grasscube1024.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		skyCubeTex->Filename.c_str(), skyCubeTex->Resource, skyCubeTex->UploadHeap));

	mTextures[stoneTex->Name] = move(stoneTex);
	mTextures[waterTex->Name] = move(waterTex);
	mTextures[myCubeTex->Name] = move(myCubeTex);
	mTextures[enemyCubeTex->Name] = move(enemyCubeTex);
	mTextures[myShellTex->Name] = move(myShellTex);
	mTextures[skyCubeTex->Name] = move(skyCubeTex);
}

void FlyingCrates::SetRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);				// descriptor table type: shader resource view for textures, shader register 0  in hlsl

	CD3DX12_DESCRIPTOR_RANGE skyTexTable;					
	skyTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);			// descriptor table type: shader resource view for a texture cube, shader register 1 in hlsl

	CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);			// it indicates Texture2D gDiffuseMap : register(t0) in hlsl
	slotRootParameter[1].InitAsConstantBufferView(0);													// it indicates cbObject : register(b0) in hlsl
	slotRootParameter[2].InitAsConstantBufferView(1);													// it indicates cbCommon : register(b1) in hlsl
	slotRootParameter[3].InitAsConstantBufferView(2);													// it indicates cbMaterial : register(b2) in hlsl
	slotRootParameter[4].InitAsDescriptorTable(1, &skyTexTable, D3D12_SHADER_VISIBILITY_PIXEL);			// it indicate TextureCube gCubeMap : register(t1) in hlsl

	auto staticSamplers = GetStaticSamplers();

	// description for creating a root signature object
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, (UINT)staticSamplers.size(),
		staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	// check if any error has occurred.
	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void FlyingCrates::SetDescriptorHeaps()
{
	// create shader resource view heap.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 6;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// fill out the heap with actual descriptors.

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto stoneTex = mTextures["stoneTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto myCubeTex = mTextures["myCubeTex"]->Resource;
	auto enemyCubeTex = mTextures["enemyCubeTex"]->Resource;
	auto myShellTex = mTextures["myShellTex"]->Resource;
	auto skyCubeTex = mTextures["skyTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = stoneTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = waterTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = waterTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = myCubeTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = myCubeTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(myCubeTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = enemyCubeTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = enemyCubeTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(enemyCubeTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = myShellTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = myShellTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(myShellTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyCubeTex->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyCubeTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyCubeTex.Get(), &srvDesc, hDescriptor);

	mSkyCubeTexHeapIndex = 5;
}

void FlyingCrates::SetShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\BasicShader.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\BasicShader.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void FlyingCrates::SetTerrainGeometry()
{
	// first create a flat grid where a vertex lies at each intersecting point.
	// then add change in the y-component of each vertex to obtain the terrain mesh geometry. 
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(400.0f, 400.0f, 50, 50);

	vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z) + MathHelper::RandF(-2.0f, 2.0f);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	vector<uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "terrainGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["terrainGeo"] = move(geo);
}

void FlyingCrates::SetWaterGeometry()
{
	// set up index buffer first, vertices are not fixed. they changes dynamically
	vector<uint16_t> indices(3 * mWaterSurface->GetTriangleCount());		// 3 indices per face
	assert(mWaterSurface->GetVertexCount() < 0x0000ffff);

	// iterate over each quad.
	int row = mWaterSurface->GetRowCount();
	int col = mWaterSurface->GetColumnCount();
	int k = 0;
	for (int i = 0; i < row - 1; ++i)
	{
		for (int j = 0; j < col - 1; ++j, k += 6)
		{
			indices[k] = i * col + j;
			indices[k + 1] = i * col + j + 1;
			indices[k + 2] = (i + 1) * col + j;

			indices[k + 3] = (i + 1) * col + j;
			indices[k + 4] = i * col + j + 1;
			indices[k + 5] = (i + 1) * col + j + 1;
		}
	}

	UINT vbByteSize = mWaterSurface->GetVertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	geo->VertexBufferCPU = nullptr;		// do not set up vertex buffer here
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = move(geo);
}

void FlyingCrates::SetFiguresGeometry()
{
	// two figures: box, sphere
	// concatenate their separate vertices and indices buffer into one vertices and indices buffer. 
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(3.0f, 20, 20);

	UINT boxVertexStart = 0;
	UINT sphereVertexStart = (UINT)box.Vertices.size();

	UINT boxIndexStart = 0;
	UINT sphereIndexStart = (UINT)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexStart;
	boxSubmesh.BaseVertexLocation = boxVertexStart;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexStart;
	sphereSubmesh.BaseVertexLocation = sphereVertexStart;

	auto totalVertexCount = box.Vertices.size() + sphere.Vertices.size();

	vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	vector<uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "figureGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void FlyingCrates::SetPSOs()
{
	// pipeline state object for opaque objects.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// PSO for player object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC playerPsoDesc = opaquePsoDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&playerPsoDesc, IID_PPV_ARGS(&mPSOs["player"])));

	// PSO for shell fired
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shellPsoDesc = opaquePsoDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shellPsoDesc, IID_PPV_ARGS(&mPSOs["shell"])));

	// PSO for enemy object
	D3D12_GRAPHICS_PIPELINE_STATE_DESC enemyPsoDesc = opaquePsoDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&enemyPsoDesc, IID_PPV_ARGS(&mPSOs["enemy"])));

	// pipeline state object for transparent objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	// pipeline state object for sky
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;

	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}


void FlyingCrates::SetFrameBuffers()
{
	for (int i = 0; i < gNumFrameBuffers; ++i)
	{
		mFrameBuffers.push_back(make_unique<FrameBuffer>(md3dDevice.Get(), 1,
			(UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaterSurface->GetVertexCount()));
	}
}

void FlyingCrates::SetMaterials()
{
	auto stone = make_unique<Material>();
	stone->Name = "stone";
	stone->MatCBIndex = 0;
	stone->DiffuseSrvHeapIndex = 0;
	stone->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	stone->Roughness = 0.125f;

	auto water = make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.75f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto mycube = make_unique<Material>();
	mycube->Name = "mycube";
	mycube->MatCBIndex = 2;
	mycube->DiffuseSrvHeapIndex = 2;
	mycube->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	mycube->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	mycube->Roughness = 0.5f;

	auto enemycube = make_unique<Material>();
	enemycube->Name = "enemycube";
	enemycube->MatCBIndex = 3;
	enemycube->DiffuseSrvHeapIndex = 3;
	enemycube->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	enemycube->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	enemycube->Roughness = 0.5f;

	auto myshell = make_unique<Material>();
	myshell->Name = "myshell";
	myshell->MatCBIndex = 4;
	myshell->DiffuseSrvHeapIndex = 4;
	myshell->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.7f);
	myshell->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	myshell->Roughness = 0.1f;

	auto sky = make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = 5;
	sky->DiffuseSrvHeapIndex = mSkyCubeTexHeapIndex;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["stone"] = move(stone);
	mMaterials["water"] = move(water);
	mMaterials["mycube"] = move(mycube);
	mMaterials["enemycube"] = move(enemycube);
	mMaterials["myshell"] = move(myshell);
	mMaterials["sky"] = move(sky);
}

void FlyingCrates::SetRenderingItems()
{
	// water surface
	UINT itemIndex = 0;
	auto waterRitem = make_unique<RenderItem>();
	waterRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&waterRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	waterRitem->isItemStatic = false;
	waterRitem->ObjCBIndex = itemIndex;
	waterRitem->Mat = mMaterials["water"].get();
	waterRitem->Geo = mGeometries["waterGeo"].get();
	waterRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	waterRitem->IndexCount = waterRitem->Geo->DrawArgs["grid"].IndexCount;
	waterRitem->StartIndexLocation = waterRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	waterRitem->BaseVertexLocation = waterRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mWaterRitem = waterRitem.get();
	mRitemLayer[(int)RenderLayer::Transparent].push_back(waterRitem.get());
	mAllRitems.push_back(move(waterRitem));
	itemIndex++;

	// terrain 
	auto terrainRitem = make_unique<RenderItem>();
	terrainRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&terrainRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	terrainRitem->isItemStatic = true;
	terrainRitem->ObjCBIndex = itemIndex;
	terrainRitem->Mat = mMaterials["stone"].get();
	terrainRitem->Geo = mGeometries["terrainGeo"].get();
	terrainRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	terrainRitem->IndexCount = terrainRitem->Geo->DrawArgs["grid"].IndexCount;
	terrainRitem->StartIndexLocation = terrainRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	terrainRitem->BaseVertexLocation = terrainRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(terrainRitem.get());
	mAllRitems.push_back(move(terrainRitem));
	itemIndex++;
	
	// player's crate
	auto playerRitem = make_unique<RenderItem>();
	XMStoreFloat4x4(&playerRitem->World,
		XMMatrixScaling(mPlayer.playerScale, mPlayer.playerScale, mPlayer.playerScale)
		* XMMatrixTranslation(mPlayer.plPosition.x, mPlayer.plPosition.y, mPlayer.plPosition.z));		// initial position of the player: (0, 50, -200)
	XMStoreFloat4x4(&playerRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	playerRitem->isItemStatic = false;
	playerRitem->ObjCBIndex = itemIndex;
	playerRitem->Mat = mMaterials["mycube"].get();
	playerRitem->Geo = mGeometries["figureGeo"].get();
	playerRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	playerRitem->IndexCount = playerRitem->Geo->DrawArgs["box"].IndexCount;
	playerRitem->StartIndexLocation = playerRitem->Geo->DrawArgs["box"].StartIndexLocation;
	playerRitem->BaseVertexLocation = playerRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Player].push_back(playerRitem.get());
	mPlayer.objPlayer = playerRitem.get();
	mAllRitems.push_back(move(playerRitem));
	itemIndex++;

	// player's shells
	for (size_t i = 0; i < 5; ++i)
	{
		auto shellRitem = make_unique<RenderItem>();
		XMStoreFloat4x4(&shellRitem->World, XMMatrixScaling(1.0f, 1.0f, 1.0f) *
			XMMatrixTranslation(mPlayer.plPosition.x, mPlayer.plPosition.y, mPlayer.plPosition.z));
		XMStoreFloat4x4(&shellRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
		shellRitem->isItemStatic = false;
		shellRitem->isItemActivated = false;	// all 5 shells in the magazine are initially inactive.
		shellRitem->ObjCBIndex = itemIndex;
		shellRitem->Mat = mMaterials["myshell"].get();
		shellRitem->Geo = mGeometries["figureGeo"].get();
		shellRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		shellRitem->IndexCount = shellRitem->Geo->DrawArgs["sphere"].IndexCount;
		shellRitem->StartIndexLocation = shellRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		shellRitem->BaseVertexLocation = shellRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Shell].push_back(shellRitem.get());
		mPlayer.myShell.rpShell[i] = shellRitem.get();
		mAllRitems.push_back(move(shellRitem));
		itemIndex++;
	}

	// enemy cubes
	for (size_t i = 0; i < 5; ++i)
	{
		auto enemyRitem = make_unique<RenderItem>();
		XMStoreFloat4x4(&enemyRitem->World, XMMatrixScaling(15.0f, 15.0f, 15.0f) * XMMatrixTranslation(-100.0f + (float)i * 50.0f, 50.0f, +300.0f));
		XMStoreFloat4x4(&enemyRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
		enemyRitem->isItemStatic = false;
		enemyRitem->isItemActivated = false;
		enemyRitem->ObjCBIndex = itemIndex;
		enemyRitem->Mat = mMaterials["enemycube"].get();
		enemyRitem->Geo = mGeometries["figureGeo"].get();
		enemyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		enemyRitem->IndexCount = enemyRitem->Geo->DrawArgs["box"].IndexCount;
		enemyRitem->StartIndexLocation = enemyRitem->Geo->DrawArgs["box"].StartIndexLocation;
		enemyRitem->BaseVertexLocation = enemyRitem->Geo->DrawArgs["box"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Enemy].push_back(enemyRitem.get());
		mEnemy.rpEnemy[i] = enemyRitem.get();
		mAllRitems.push_back(move(enemyRitem));
		itemIndex++;
	}

	auto skyRitem = make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(1000.0f, 1000.0f, 1000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->isItemStatic = true;
	skyRitem->ObjCBIndex = itemIndex;
	skyRitem->Mat = mMaterials["sky"].get();
	skyRitem->Geo = mGeometries["figureGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(move(skyRitem));
}

array<const CD3DX12_STATIC_SAMPLER_DESC, 6> FlyingCrates::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers. So just define them all up front
	// and keep them available as part of the root signature.

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}

float FlyingCrates::GetHillsHeight(float x, float z) const
{

	return mPeakHeight * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 FlyingCrates::GetHillsNormal(float x, float z) const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-mPeakHeight / 10.0f * z * cosf(0.1f * x) - mPeakHeight * cosf(0.1f * z),
		1.0f,
		-mPeakHeight * sinf(0.1f * x) + mPeakHeight / 10.0f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

bool CloseEnough(WPosition pt1, WPosition pt2, float dist)
{
	float distCheckSq = (pt1.x - pt2.x) * (pt1.x - pt2.x) +
		(pt1.y - pt2.y) * (pt1.y - pt2.y) + (pt1.z - pt2.z) * (pt1.z - pt2.z);

	float distSq = dist * dist;

	return distCheckSq < distSq ? true : false;
}

