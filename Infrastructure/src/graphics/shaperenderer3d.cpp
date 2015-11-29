
#include "graphics/bufferbinding.h"
#include "graphics/shaperenderer3d.h"

// When drawing circles, how many segments do we use?
constexpr static int sCircleSegments = 74;

namespace gfx {	
	
	struct ShapeRenderer3d::Impl : ResourceListener {
		Impl(RenderingDevice& device);
		
		std::array<ShapeVertex3d, 16> discVerticesTpl;
		VertexBufferPtr discVertexBuffer;
		IndexBufferPtr discIndexBuffer;
		BufferBinding discBufferBinding;
		Material quadMaterial;
		Material lineMaterial;
		Material lineOccludedMaterial;
		VertexBufferPtr lineVertexBuffer;
		BufferBinding lineBinding;

		VertexBufferPtr circleVertexBuffer;
		BufferBinding circleBinding;

		RenderingDevice &device;
		
		ResourceListenerRegistration mRegistration;

		void CreateResources(RenderingDevice&) override;
		void FreeResources(RenderingDevice&) override;

		static Material CreateLineMaterial(RenderingDevice &device, bool occludedOnly);
		static Material CreateQuadMaterial(RenderingDevice &device);

		void BindLineMaterial(XMCOLOR color, bool occludedOnly = false);
		void BindQuadMaterial(XMCOLOR color, const gfx::TextureRef &texture);
	};

	void ShapeRenderer3d::Impl::CreateResources(RenderingDevice&) {

		discVerticesTpl[0].pos = { -1.0f, 0.0f, -1.0f };
		discVerticesTpl[1].pos = { 0.0f, 0.0f, -1.0f };
		discVerticesTpl[2].pos = { 0.0f, 0.0f, 0.0f };
		discVerticesTpl[3].pos = { -1.0f, 0.0f, 0.0f };
		discVerticesTpl[4].pos = { 0.0f, 0.0f, -1.0f };
		discVerticesTpl[5].pos = { 1.0f, 0.0f, -1.0f };
		discVerticesTpl[6].pos = { 1.0f, 0.0f, 0.0f };
		discVerticesTpl[7].pos = { 0.0f, 0.0f, 0.0f };
		discVerticesTpl[8].pos = { 0.0f, 0.0f, 0.0f };
		discVerticesTpl[9].pos = { 1.0f, 0.0f, 0.0f };
		discVerticesTpl[10].pos = { 1.0f, 0.0f, 1.0f };
		discVerticesTpl[11].pos = { 0.0f, 0.0f, 1.0f };
		discVerticesTpl[12].pos = { -1.0f, 0.0f, 0.0f };
		discVerticesTpl[13].pos = { 0.0f, 0.0f, 0.0f };
		discVerticesTpl[14].pos = { 0.0f, 0.0f, 1.0f };
		discVerticesTpl[15].pos = { -1.0f, 0.0f, 1.0f };

		discVerticesTpl[0].uv = { 0, 0 };
		discVerticesTpl[1].uv = { 1, 0 };
		discVerticesTpl[2].uv = { 1, 1 };
		discVerticesTpl[3].uv = { 0, 1 };
		discVerticesTpl[4].uv = { 0, 1 };
		discVerticesTpl[5].uv = { 0, 0 };
		discVerticesTpl[6].uv = { 1, 0 };
		discVerticesTpl[7].uv = { 1, 1 };
		discVerticesTpl[8].uv = { 1, 1 };
		discVerticesTpl[9].uv = { 0, 1 };
		discVerticesTpl[10].uv = { 0, 0 };
		discVerticesTpl[11].uv = { 1, 0 };
		discVerticesTpl[12].uv = { 1, 0 };
		discVerticesTpl[13].uv = { 1, 1 };
		discVerticesTpl[14].uv = { 0, 1 };
		discVerticesTpl[15].uv = { 0, 0 };

		std::array<uint16_t, 24> indices{
			0, 2, 1,
			0, 3, 2,
			4, 6, 5,
			4, 7, 6, 
			8, 10, 9,
			8, 11, 10,
			12, 14, 13,
			12, 15, 14
		};
		discIndexBuffer = device.CreateIndexBuffer(indices);

		discVertexBuffer = device.CreateEmptyVertexBuffer(sizeof(ShapeVertex3d) * 16);

		discBufferBinding.AddBuffer(discVertexBuffer, 0, sizeof(ShapeVertex3d))
			.AddElement(VertexElementType::Float3, VertexElementSemantic::Position)
			.AddElement(VertexElementType::Float2, VertexElementSemantic::TexCoord);

		// +3 because for n lines, you need n+1 points and in this case, we have to repeat
		// the first point again to close the loop (so +2) and also include the center point
		// to draw a circle at the end (+3)
		circleVertexBuffer = device.CreateEmptyVertexBuffer(sizeof(XMFLOAT3) * (sCircleSegments + 3));

		circleBinding.AddBuffer(circleVertexBuffer, 0, sizeof(XMFLOAT3))
			.AddElement(VertexElementType::Float3, VertexElementSemantic::Position);

		// Just the two end points of a line
		lineVertexBuffer = device.CreateEmptyVertexBuffer(sizeof(XMFLOAT3) * 2);
		lineBinding.AddBuffer(lineVertexBuffer, 0, sizeof(XMFLOAT3))
			.AddElement(VertexElementType::Float3, VertexElementSemantic::Position);

	}

	void ShapeRenderer3d::Impl::FreeResources(RenderingDevice&) {
		discVertexBuffer.reset();
		discIndexBuffer.reset();
	}

	Material ShapeRenderer3d::Impl::CreateLineMaterial(RenderingDevice& device, bool occludedOnly) {
		BlendState blendState;
		blendState.blendEnable = true;
		blendState.srcBlend = D3DBLEND_SRCALPHA;
		blendState.destBlend = D3DBLEND_INVSRCALPHA;
		DepthStencilState depthState;
		depthState.depthEnable = true;
		depthState.depthWrite = false;
		if (occludedOnly) {
			depthState.depthFunc = D3DCMP_GREATEREQUAL;
		}
		RasterizerState rasterizerState;
		auto vs = device.GetShaders().LoadVertexShader("line_vs");
		auto ps = device.GetShaders().LoadPixelShader("diffuse_only_ps");
		return Material(blendState, depthState, rasterizerState, {}, vs, ps);
	}

	Material ShapeRenderer3d::Impl::CreateQuadMaterial(RenderingDevice & device)
	{
		BlendState blendState;
		blendState.blendEnable = true;
		blendState.srcBlend = D3DBLEND_SRCALPHA;
		blendState.destBlend = D3DBLEND_INVSRCALPHA;
		DepthStencilState depthState;
		depthState.depthEnable = true;
		depthState.depthWrite = false;
		RasterizerState rasterizerState;
		rasterizerState.cullMode = D3DCULL_NONE;
		Shaders::ShaderDefines vsDefines;
		vsDefines["TEXTURE_STAGES"] = "1";
		auto vs = device.GetShaders().LoadVertexShader("mdf_vs", vsDefines);
		auto ps = device.GetShaders().LoadPixelShader("textured_simple_ps");
		SamplerState samplerState;
		samplerState.addressU = D3DTADDRESS_CLAMP;
		samplerState.addressV = D3DTADDRESS_CLAMP;
		samplerState.minFilter = D3DTEXF_LINEAR;
		samplerState.magFilter = D3DTEXF_LINEAR;
		samplerState.mipFilter = D3DTEXF_LINEAR;
		std::vector<MaterialSamplerBinding> samplers{ { nullptr, samplerState } };
		return Material(blendState, depthState, rasterizerState, samplers, vs, ps);
	}

	void ShapeRenderer3d::Impl::BindLineMaterial(XMCOLOR color, bool occludedOnly) {
		if (occludedOnly) {
			device.SetMaterial(lineOccludedMaterial);
		} else {
			device.SetMaterial(lineMaterial);
		}

		device.GetDevice()->SetVertexShaderConstantF(0, &device.GetCamera().GetViewProj()._11, 4);
		XMFLOAT4 colors;
		XMStoreFloat4(&colors, PackedVector::XMLoadColor(&color));
		device.GetDevice()->SetVertexShaderConstantF(4, &colors.x, 4);
	}

	void ShapeRenderer3d::Impl::BindQuadMaterial(XMCOLOR color, const gfx::TextureRef & texture)
	{
		device.SetMaterial(quadMaterial);
		device.GetDevice()->SetVertexShaderConstantF(0, &device.GetCamera().GetViewProj()._11, 4);
		XMFLOAT4 colors;
		XMStoreFloat4(&colors, PackedVector::XMLoadColor(&color));
		colors = XMFLOAT4(1, 1, 1, 1);
		device.GetDevice()->SetVertexShaderConstantF(4, &colors.x, 4);

		device.GetDevice()->SetTexture(0, texture->GetDeviceTexture());
	}

	ShapeRenderer3d::Impl::Impl(RenderingDevice& device)
		: device(device), 
		  mRegistration(device, this), 
		  lineMaterial(CreateLineMaterial(device, false)),
		  quadMaterial(CreateQuadMaterial(device)),
		  lineOccludedMaterial(CreateLineMaterial(device, true)) {
	}

	ShapeRenderer3d::ShapeRenderer3d(RenderingDevice& device)
	 : mImpl(std::make_unique<Impl>(device)) {
	}

	ShapeRenderer3d::~ShapeRenderer3d() {
	}

	void ShapeRenderer3d::DrawDisc(
		const XMFLOAT3& center, 
		float rotation, 
		float radius,
		gfx::MdfRenderMaterialPtr& material) {

		std::array<ShapeVertex3d, 16> vertices = mImpl->discVerticesTpl;

		// There is some sort of rotation going on here that is related to 
		// the view transformation
		auto v8 = cosf(-0.77539754f) * -1.5f;
		auto v9 = sinf(-0.77539754f) * -1.5f;

		for (size_t i = 0; i < 16; ++i) {

			auto orgx = mImpl->discVerticesTpl[i].pos.x;
			auto orgz = mImpl->discVerticesTpl[i].pos.z;

			// The cos/sin magic rotates around the Y axis
			vertices[i].pos.x = sinf(rotation) * orgz + cosf(rotation) * orgx;
			vertices[i].pos.y = v9;
			vertices[i].pos.z = cosf(rotation) * orgz - sinf(rotation) * orgx;
			
			// Scale is being applied here
			vertices[i].pos.x = radius * vertices[i].pos.x;
			vertices[i].pos.z = radius * vertices[i].pos.z;

			vertices[i].pos.x = center.x + cosf(2.3561945f) * v8 + vertices[i].pos.x;
			vertices[i].pos.z = center.z + cosf(2.3561945f) * v8 + vertices[i].pos.z;
		}

		mImpl->discVertexBuffer->Update<ShapeVertex3d>(vertices);
		mImpl->discBufferBinding.Bind();
		material->Bind(mImpl->device, {});

		mImpl->device.GetDevice()->SetIndices(mImpl->discIndexBuffer->GetBuffer());
		mImpl->device.GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 16, 0, 8);

	}

	void ShapeRenderer3d::DrawQuad(const std::array<ShapeVertex3d, 4> &corners,
		XMCOLOR color,
		const gfx::TextureRef &texture) {

		mImpl->discVertexBuffer->Update<const ShapeVertex3d>(corners);
		mImpl->discBufferBinding.Bind();

		mImpl->BindQuadMaterial(color, texture);

		mImpl->device.GetDevice()->SetIndices(mImpl->discIndexBuffer->GetBuffer());
		mImpl->device.GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

	}

	void ShapeRenderer3d::DrawQuad(const std::array<ShapeVertex3d, 4> &corners,
		gfx::MdfRenderMaterial &material,
		XMCOLOR color) {

		mImpl->discVertexBuffer->Update<const ShapeVertex3d>(corners);
		mImpl->discBufferBinding.Bind();

		MdfRenderOverrides overrides;
		overrides.overrideDiffuse = true;
		overrides.overrideColor = color;
		material.Bind(mImpl->device, {}, &overrides);

		mImpl->device.GetDevice()->SetIndices(mImpl->discIndexBuffer->GetBuffer());
		mImpl->device.GetDevice()->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

	}

	void ShapeRenderer3d::DrawLine(const XMFLOAT3& from, const XMFLOAT3& to, XMCOLOR color) {

		std::array<XMFLOAT3, 2> positions{
			from,
			to
		};

		mImpl->lineVertexBuffer->Update<XMFLOAT3>(positions);
		mImpl->lineBinding.Bind();
		mImpl->BindLineMaterial(color);
		
		mImpl->device.GetDevice()->DrawPrimitive(D3DPT_LINELIST, 0, 1);

	}

	void ShapeRenderer3d::DrawFilledCircle(const XMFLOAT3& center, float radius, XMCOLOR borderColor, XMCOLOR fillColor, bool occludedOnly) {

		// The positions array contains the following:
		// 0 -> The center of the circle
		// 1 - (sCircleSegments + 1) -> Positions on the diameter of the circle
		// sCircleSegments + 2 -> The first position again to close the circle
		std::array<XMFLOAT3, sCircleSegments + 3> positions;

		// A full rotation divided by the number of segments
		auto rotPerSegment = XM_2PI / sCircleSegments;

		positions[0] = center;
		for (auto i = 1; i < sCircleSegments + 3; ++i) {
			auto rot = (sCircleSegments - i) * rotPerSegment;
			positions[i].x = center.x + cosf(rot) * radius - sinf(rot) * 0.0f;
			positions[i].y = center.y;
			positions[i].z = center.z + cosf(rot) * 0.0f + sinf(rot) * radius;
		}
		positions.back() = positions[1];
		
		mImpl->circleVertexBuffer->Update<XMFLOAT3>(positions);
		mImpl->circleBinding.Bind();
		
		mImpl->BindLineMaterial(borderColor, occludedOnly);			

		// The first vertex is the center vertex, so we skip it for line drawing
		mImpl->device.GetDevice()->DrawPrimitive(D3DPT_LINESTRIP, 1, sCircleSegments);

		mImpl->BindLineMaterial(fillColor, occludedOnly);

		mImpl->device.GetDevice()->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, sCircleSegments);

	}

	static constexpr float cos45 = 0.70709997f;

	void ShapeRenderer3d::DrawCylinder(const XMFLOAT3 &pos, float radius, float height) {
		// Draw the 3d bounding cylinder of the object
		static XMCOLOR diffuse(0, 1.0f, 0, 0.5f);

		float x = pos.x;
		float y = pos.y;
		float z = pos.z;

		auto scaledRadius = radius * cos45;
		XMFLOAT3 from, to;

		from.x = x + scaledRadius;
		from.y = y;
		from.z = z - scaledRadius;
		to.x = from.x;
		to.y = y + height;
		to.z = from.z;

		DrawLine(from, to, diffuse);

		from.x = x - scaledRadius;
		from.z = z + scaledRadius;
		to.x = from.x;
		to.z = from.z;
		DrawLine(from, to, diffuse);

		/*
		Draw the circle on top and on the bottom
		of the cylinder.
		*/
		for (auto i = 0; i < 24; ++i) {
			// We rotate 360� in 24 steps of 15� each
			auto rot = i * XMConvertToRadians(15);
			auto nextRot = rot + XMConvertToRadians(15);

			// This is the bottom cap
			from.x = x + cosf(rot) * radius;
			from.y = y;
			from.z = z - sinf(rot) * radius;
			to.x = x + cosf(nextRot) * radius;
			to.y = y;
			to.z = z - sinf(nextRot) * radius;
			DrawLine(from, to, diffuse);

			// This is the top cap
			from.x = x + cosf(rot) * radius;
			from.y = y + height;
			from.z = z - sinf(rot) * radius;
			to.x = x + cosf(nextRot) * radius;
			to.y = y + height;
			to.z = z - sinf(nextRot) * radius;
			DrawLine(from, to, diffuse);
		}
	}

}
