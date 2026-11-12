	#include "main.h"
	#include "renderer.h"
	#include "animationModel.h"

	void AnimationModel::Draw()
	{
		// プリミティブトポロジ設定
		Renderer::GetDeviceContext()->IASetPrimitiveTopology(
			D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// マテリアル設定
		MATERIAL material;
		ZeroMemory(&material, sizeof(material));
		material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material.TextureEnable = true;
		Renderer::SetMaterial(material);

		for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
		{
			aiMesh* mesh = m_AiScene->mMeshes[m];


			// マテリアル設定
			aiString texture;
			aiColor3D diffuse;
			float opacity;

			aiMaterial* aimaterial = m_AiScene->mMaterials[mesh->mMaterialIndex];
			aimaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texture);
			aimaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			aimaterial->Get(AI_MATKEY_OPACITY, opacity);

			if (texture == aiString(""))
			{
				material.TextureEnable = false;
			}
			else
			{
				Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture[texture.data]);
				material.TextureEnable = true;
			}

			material.Diffuse = XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, opacity);
			material.Ambient = material.Diffuse;
			Renderer::SetMaterial(material);


			// 頂点バッファ設定
			UINT stride = sizeof(VERTEX_3D);
			UINT offset = 0;
			Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer[m], &stride, &offset);

			// インデックスバッファ設定
			Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer[m], DXGI_FORMAT_R32_UINT, 0);

			// ポリゴン描画
			Renderer::GetDeviceContext()->DrawIndexed(mesh->mNumFaces * 3, 0, 0);
		}
	}

	void AnimationModel::Load(const char* FileName)
	{
		const std::string modelPath(FileName);

		m_AiScene = aiImportFile(FileName, aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded);
		assert(m_AiScene);

		m_VertexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];
		m_IndexBuffer = new ID3D11Buffer * [m_AiScene->mNumMeshes];


		//変形後頂点配列生成
		m_DeformVertex = new std::vector<DEFORM_VERTEX>[m_AiScene->mNumMeshes];

		//再帰的にボーン生成
		CreateBone(m_AiScene->mRootNode);



		for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
		{
			aiMesh* mesh = m_AiScene->mMeshes[m];

			// 頂点バッファ生成
			{
				VERTEX_3D* vertex = new VERTEX_3D[mesh->mNumVertices];

				for (unsigned int v = 0; v < mesh->mNumVertices; v++)
				{
					vertex[v].Position = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
					vertex[v].Normal = XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
					vertex[v].TexCoord = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
					vertex[v].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}

				D3D11_BUFFER_DESC bd;
				ZeroMemory(&bd, sizeof(bd));
				bd.Usage = D3D11_USAGE_DYNAMIC;
				bd.ByteWidth = sizeof(VERTEX_3D) * mesh->mNumVertices;
				bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

				D3D11_SUBRESOURCE_DATA sd;
				ZeroMemory(&sd, sizeof(sd));
				sd.pSysMem = vertex;

				Renderer::GetDevice()->CreateBuffer(&bd, &sd,
					&m_VertexBuffer[m]);

				delete[] vertex;
			}


			// インデックスバッファ生成
			{
				unsigned int* index = new unsigned int[mesh->mNumFaces * 3];

				for (unsigned int f = 0; f < mesh->mNumFaces; f++)
				{
					const aiFace* face = &mesh->mFaces[f];

					assert(face->mNumIndices == 3);

					index[f * 3 + 0] = face->mIndices[0];
					index[f * 3 + 1] = face->mIndices[1];
					index[f * 3 + 2] = face->mIndices[2];
				}

				D3D11_BUFFER_DESC bd;
				ZeroMemory(&bd, sizeof(bd));
				bd.Usage = D3D11_USAGE_DEFAULT;
				bd.ByteWidth = sizeof(unsigned int) * mesh->mNumFaces * 3;
				bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
				bd.CPUAccessFlags = 0;

				D3D11_SUBRESOURCE_DATA sd;
				ZeroMemory(&sd, sizeof(sd));
				sd.pSysMem = index;

				Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_IndexBuffer[m]);

				delete[] index;
			}



			//変形後頂点データ初期化
			for (unsigned int v = 0; v < mesh->mNumVertices; v++)
			{
				DEFORM_VERTEX deformVertex;
				deformVertex.Position = mesh->mVertices[v];
				deformVertex.Normal = mesh->mNormals[v];
				deformVertex.BoneNum = 0;

				for (unsigned int b = 0; b < 4; b++)
				{
					deformVertex.BoneName[b] = "";
					deformVertex.BoneWeight[b] = 0.0f;
				}

				m_DeformVertex[m].push_back(deformVertex);
			}


			//ボーンデータ初期化
			for (unsigned int b = 0; b < mesh->mNumBones; b++)
			{
				aiBone* bone = mesh->mBones[b];

				m_Bone[bone->mName.C_Str()].OffsetMatrix = bone->mOffsetMatrix;

				//変形後頂点にボーンデータ格納
				for (unsigned int w = 0; w < bone->mNumWeights; w++)
				{
					aiVertexWeight weight = bone->mWeights[w];

					int num = m_DeformVertex[m][weight.mVertexId].BoneNum;

					m_DeformVertex[m][weight.mVertexId].BoneWeight[num] = weight.mWeight;
					m_DeformVertex[m][weight.mVertexId].BoneName[num] = bone->mName.C_Str();
					m_DeformVertex[m][weight.mVertexId].BoneNum++;

					assert(m_DeformVertex[m][weight.mVertexId].BoneNum <= 4);
				}
			}
		}



		//テクスチャ読み込み
		for (unsigned int i = 0; i < m_AiScene->mNumTextures; i++)
		{
			aiTexture* aitexture = m_AiScene->mTextures[i];

			ID3D11ShaderResourceView* texture;

			// テクスチャ読み込み
			TexMetadata metadata;
			ScratchImage image;
			LoadFromWICMemory(aitexture->pcData, aitexture->mWidth, WIC_FLAGS_NONE, &metadata, image);
			CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(), image.GetImageCount(), metadata, &texture);
			assert(texture);

			m_Texture[aitexture->mFilename.data] = texture;
		}



	}



	void AnimationModel::LoadAnimation(const char* FileName, const char* Name)
	{

		m_Animation[Name] = aiImportFile(FileName, aiProcess_ConvertToLeftHanded);
		assert(m_Animation[Name]);

	}


	void AnimationModel::CreateBone(aiNode* node)
	{
		BONE bone;

		m_Bone[node->mName.C_Str()] = bone;

		for (unsigned int n = 0; n < node->mNumChildren; n++)
		{
			CreateBone(node->mChildren[n]);
		}

	}


	void AnimationModel::Uninit()
	{
		for (unsigned int m = 0; m < m_AiScene->mNumMeshes; m++)
		{
			m_VertexBuffer[m]->Release();
			m_IndexBuffer[m]->Release();
		}

		delete[] m_VertexBuffer;
		delete[] m_IndexBuffer;

		delete[] m_DeformVertex;


		for (std::pair<const std::string, ID3D11ShaderResourceView*> pair : m_Texture)
		{
			pair.second->Release();
		}



		aiReleaseImport(m_AiScene);


		for (std::pair<const std::string, const aiScene*> pair : m_Animation)
		{
			aiReleaseImport(pair.second);
		}

	}





	void AnimationModel::Update(const char* AnimationName1, int Frame1,const char* AnimationName2, int Frame2, float Blend)
	{
		if (m_Animation.count(AnimationName1) == 0)
			return;

		if (!m_Animation[AnimationName1]->HasAnimations())
			return;

		if (m_Animation.count(AnimationName2) == 0)
			return;

		if (!m_Animation[AnimationName2]->HasAnimations())
			return;

		//アニメーションデータからボーンマトリクス算出

		aiAnimation* animation1 = m_Animation[AnimationName1]->mAnimations[0];
		aiAnimation* animation2 = m_Animation[AnimationName2]->mAnimations[0];

		for (auto pair : m_Bone)
		{
			BONE* bone = &m_Bone[pair.first];

			aiNodeAnim* nodeAnim1 = nullptr;
			for (unsigned int c = 0; c < animation1->mNumChannels; c++)
			{
				if (animation1->mChannels[c]->mNodeName == aiString(pair.first))
				{
					nodeAnim1 = animation1->mChannels[c];
					break;
				}
			}

			aiNodeAnim* nodeAnim2 = nullptr;
			for (unsigned int c = 0; c < animation2->mNumChannels; c++)
			{
				if (animation2->mChannels[c]->mNodeName == aiString(pair.first))
				{
					nodeAnim2 = animation2->mChannels[c];
					break;
				}
			}

			int f;
			aiQuaternion rot1;
			aiVector3D pos1;

			if (nodeAnim1)
			{
				f = Frame1 % nodeAnim1->mNumRotationKeys;
				rot1 = nodeAnim1->mRotationKeys[f].mValue;
				f = Frame1 % nodeAnim1->mNumPositionKeys;
				pos1 = nodeAnim1->mPositionKeys[f].mValue;
			}

			aiQuaternion rot2;
			aiVector3D pos2;

			if (nodeAnim2)
			{
				f = Frame2 % nodeAnim2->mNumRotationKeys;
				rot2 = nodeAnim2->mRotationKeys[f].mValue;
				f = Frame2 % nodeAnim2->mNumPositionKeys;
				pos2 = nodeAnim2->mPositionKeys[f].mValue;
			}

			aiVector3D pos = pos1 * (1.0f - Blend) + pos2 * Blend;

			aiQuaternion rot;
			aiQuaternion::Interpolate(rot, rot1, rot2, Blend);

			bone->AnimationMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f), rot, pos);
		}
			//再帰的にボーンマトリクスを更新

			aiMatrix4x4 rootMatrix = aiMatrix4x4(aiVector3D(1.0f, 1.0f, 1.0f),
			aiQuaternion((float)AI_MATH_PI, 0.0f, 0.0f), aiVector3D(0.0f, 0.0f, 0.0f));

			UpdateBoneMatrix(m_AiScene->mRootNode, rootMatrix);

			for (unsigned int n = 0; n < m_AiScene->mNumMeshes; n++)
			{
				aiMesh* mesh = m_AiScene->mMeshes[n];
				D3D11_MAPPED_SUBRESOURCE ms;
				Renderer::GetDeviceContext()->Map(m_VertexBuffer[n], 0,
					D3D11_MAP_WRITE_DISCARD, 0, &ms);

				VERTEX_3D* vertex = (VERTEX_3D*)ms.pData;

				for (unsigned int v = 0; v < mesh->mNumVertices; v++)
				{
					DEFORM_VERTEX* deformVertex = &m_DeformVertex[n][v];
					aiMatrix4x4 matrix[4];
					matrix[0] = m_Bone[deformVertex->BoneName[0]].Matrix;
					matrix[1] = m_Bone[deformVertex->BoneName[1]].Matrix;
					matrix[2] = m_Bone[deformVertex->BoneName[2]].Matrix;
					matrix[3] = m_Bone[deformVertex->BoneName[3]].Matrix;

					aiMatrix4x4 outMatrix;
					outMatrix = matrix[0] * deformVertex->BoneWeight[0]
						+ matrix[1] * deformVertex->BoneWeight[1]
						+ matrix[2] * deformVertex->BoneWeight[2]
						+ matrix[3] * deformVertex->BoneWeight[3];

					deformVertex->Position = mesh->mVertices[v];
					deformVertex->Position *= outMatrix;

					outMatrix.a4 = 0.0f;
					outMatrix.b4 = 0.0f;
					outMatrix.c4 = 0.0f;

					deformVertex->Normal = mesh->mNormals[v];
					deformVertex->Normal *= outMatrix;

					vertex[v].Position.x = deformVertex->Position.x;
					vertex[v].Position.y = deformVertex->Position.y;
					vertex[v].Position.z = deformVertex->Position.z;

					vertex[v].Normal.x = deformVertex->Normal.x;
					vertex[v].Normal.y = deformVertex->Normal.y;
					vertex[v].Normal.z = deformVertex->Normal.z;

					vertex[v].TexCoord.x = mesh->mTextureCoords[0][v].x;
					vertex[v].TexCoord.y = mesh->mTextureCoords[0][v].y;

					vertex[v].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
				}
				Renderer::GetDeviceContext()->Unmap(m_VertexBuffer[n], 0);
			}

	}


	void AnimationModel::UpdateBoneMatrix(aiNode* node, aiMatrix4x4 matrix)
	{
		BONE* bone = &m_Bone[node->mName.C_Str()];

		aiMatrix4x4 worldMatrix;
		worldMatrix = matrix * bone->AnimationMatrix;

		bone->Matrix = worldMatrix * bone->OffsetMatrix;
		for (unsigned int n = 0; n < node->mNumChildren; n++)
		{
			UpdateBoneMatrix(node->mChildren[n], worldMatrix);
		}
	}

