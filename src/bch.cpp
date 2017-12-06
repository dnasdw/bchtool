#include "bch.h"
#include <png.h>
#include <PVRTextureUtilities.h>

const u32 CBch::s_uSignature = SDW_CONVERT_ENDIAN32('BCH\0');
const int CBch::s_nBPP[] = { 32, 24, 16, 16, 16, 16, 16, 8, 8, 8, 4, 4, 4, 8 };
const int CBch::s_nDecodeTransByte[64] =
{
	 0,  1,  4,  5, 16, 17, 20, 21,
	 2,  3,  6,  7, 18, 19, 22, 23,
	 8,  9, 12, 13, 24, 25, 28, 29,
	10, 11, 14, 15, 26, 27, 30, 31,
	32, 33, 36, 37, 48, 49, 52, 53,
	34, 35, 38, 39, 50, 51, 54, 55,
	40, 41, 44, 45, 56, 57, 60, 61,
	42, 43, 46, 47, 58, 59, 62, 63
};

CBch::CBch()
	: m_bVerbose(false)
{
}

CBch::~CBch()
{
}

void CBch::SetFileName(const UString& a_sFileName)
{
	m_sFileName = a_sFileName;
}

void CBch::SetDirName(const UString& a_sDirName)
{
	m_sDirName = a_sDirName;
}

void CBch::SetVerbose(bool a_bVerbose)
{
	m_bVerbose = a_bVerbose;
}

bool CBch::ExportFile()
{
	bool bResult = true;
	FILE* fp = UFopen(m_sFileName.c_str(), USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	fseek(fp, 0, SEEK_END);
	u32 uBchSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8* pBch = new u8[uBchSize];
	fread(pBch, 1, uBchSize, fp);
	fclose(fp);
	SBchHeader<SFileSectionType0>* pBchHeader = reinterpret_cast<SBchHeader<SFileSectionType0>*>(pBch);
	switch (pBchHeader->SectionOffset[0])
	{
	case sizeof(SBchHeader<SFileSectionType0>) - 4:
	case sizeof(SBchHeader<SFileSectionType0>):
		bResult = exportFile<SFileSectionType0>(pBch, SFileSectionType0::RAW);
		break;
	case sizeof(SBchHeader<SFileSectionType1>):
		bResult = exportFile<SFileSectionType1>(pBch, SFileSectionType1::RAW_EXT);
		break;
	default:
		bResult = false;
		UPrintf(USTR("ERROR: unknown bch header content section offset 0x%X\n\n"), pBchHeader->SectionOffset[0]);
		break;
	}
	delete[] pBch;
	return bResult;
}

bool CBch::ImportFile()
{
	bool bResult = true;
	FILE* fp = UFopen(m_sFileName.c_str(), USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	fseek(fp, 0, SEEK_END);
	u32 uBchSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8* pBch = new u8[uBchSize];
	fread(pBch, 1, uBchSize, fp);
	fclose(fp);
	SBchHeader<SFileSectionType0>* pBchHeader = reinterpret_cast<SBchHeader<SFileSectionType0>*>(pBch);
	switch (pBchHeader->SectionOffset[0])
	{
	case sizeof(SBchHeader<SFileSectionType0>) - 4:
	case sizeof(SBchHeader<SFileSectionType0>):
		bResult = importFile<SFileSectionType0>(pBch, SFileSectionType0::RAW);
		break;
	case sizeof(SBchHeader<SFileSectionType1>):
		bResult = importFile<SFileSectionType1>(pBch, SFileSectionType1::RAW_EXT);
		break;
	default:
		bResult = false;
		UPrintf(USTR("ERROR: unknown bch header content section offset 0x%X\n\n"), pBchHeader->SectionOffset[0]);
		break;
	}
	if (bResult)
	{
		fp = UFopen(m_sFileName.c_str(), USTR("wb"));
		if (fp != nullptr)
		{
			fwrite(pBch, 1, uBchSize, fp);
			fclose(fp);
		}
		else
		{
			bResult = false;
		}
	}
	delete[] pBch;
	return bResult;
}

bool CBch::IsBchFile(const UString& a_sFileName)
{
	FILE* fp = UFopen(a_sFileName.c_str(), USTR("rb"));
	if (fp == nullptr)
	{
		return false;
	}
	u32 uSignature = 0;
	fread(&uSignature, sizeof(uSignature), 1, fp);
	fclose(fp);
	return uSignature == s_uSignature;
}

template<typename FileSectionType>
bool CBch::exportFile(u8* a_pBch, n32 a_nL4A4Section)
{
	bool bResult = true;
	SBchHeader<FileSectionType>* pBchHeader = reinterpret_cast<SBchHeader<FileSectionType>*>(a_pBch);
	u8* pContent = a_pBch + pBchHeader->SectionOffset[FileSectionType::CONTENT];
	SPatriciaMap* pPatriciaMap = reinterpret_cast<SPatriciaMap*>(pContent);
	u32* pTextureContentOffset = reinterpret_cast<u32*>(pContent + pPatriciaMap[kContentTypeTexture].ElementsOffset);
	if (pPatriciaMap[kContentTypeTexture].Dictionary.NumData != 0)
	{
		UMkdir(m_sDirName.c_str());
	}
	for (n32 i = 0; i < pPatriciaMap[kContentTypeTexture].Dictionary.NumData; i++)
	{
		vector<STextureInfo> vTextureInfo;
		set<u32> sOffset;
		STextureContent* pTextureContent = reinterpret_cast<STextureContent*>(pContent + pTextureContentOffset[i]);
		if (pTextureContent->Format < kTextureFormatRGBA8888 || pTextureContent->Format > kTextureFormatETC1_A4)
		{
			bResult = false;
			UPrintf(USTR("ERROR: unknown format %d\n\n"), pTextureContent->Format);
			break;
		}
		for (n32 j = 0; j < SDW_ARRAY_COUNT(pTextureContent->UnitCmd); j++)
		{
			u16 uWidth = 0;
			u16 uHeight = 0;
			n32 nFormat = -1;
			vector<u32> vOffset;
			u32* pCommand = reinterpret_cast<u32*>(a_pBch + pBchHeader->SectionOffset[FileSectionType::COMMAND] + pTextureContent->UnitCmd[j].ElementsOffset);
			for (u32 k = 0; k < pTextureContent->UnitCmd[j].Count; )
			{
				if (k + 1 >= pTextureContent->UnitCmd[j].Count)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command count error\n\n"));
					break;
				}
				u32 uCommand = pCommand[k + 1];
				u32 uAddr = uCommand & 0xFFFF;
				u32 uSize = (uCommand >> 20 & 0xFF) + 1;
				if (k + static_cast<u32>(Align(uSize + 1, 2)) > pTextureContent->UnitCmd[j].Count)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command size error\n\n"));
					break;
				}
				switch (uAddr)
				{
				case PICA_REG_TEXTURE0_SIZE:
				case PICA_REG_TEXTURE1_SIZE:
				case PICA_REG_TEXTURE2_SIZE:
					uWidth = pCommand[k] >> 16 & 0xFFFF;
					uHeight = pCommand[k] & 0xFFFF;
					break;
				case PICA_REG_TEXTURE0_ADDR1:
				case PICA_REG_TEXTURE0_ADDR2:
				case PICA_REG_TEXTURE0_ADDR3:
				case PICA_REG_TEXTURE0_ADDR4:
				case PICA_REG_TEXTURE0_ADDR5:
				case PICA_REG_TEXTURE0_ADDR6:
				case PICA_REG_TEXTURE1_ADDR:
				case PICA_REG_TEXTURE2_ADDR:
					{
						u32 uOffset = pCommand[k];
						if (sOffset.insert(uOffset).second)
						{
							vOffset.push_back(uOffset);
						}
					}
					break;
				case PICA_REG_TEXTURE0_FORMAT:
				case PICA_REG_TEXTURE1_FORMAT:
				case PICA_REG_TEXTURE2_FORMAT:
					nFormat = pCommand[k];
					if (nFormat != pTextureContent->Format)
					{
						bResult = false;
						UPrintf(USTR("ERROR: command format error\n\n"));
					}
					break;
				default:
					break;
				}
				if (!bResult)
				{
					break;
				}
				k += static_cast<u32>(Align(uSize + 1, 2));
			}
			if (!bResult)
			{
				break;
			}
			if (!vOffset.empty())
			{
				if (uWidth == 0)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command no width\n\n"));
					break;
				}
				if (uHeight == 0)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command no height\n\n"));
					break;
				}
				if (nFormat == -1)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command no format\n\n"));
					break;
				}
				for (vector<u32>::iterator it = vOffset.begin(); it != vOffset.end(); ++it)
				{
					vTextureInfo.push_back(STextureInfo());
					STextureInfo& textureInfo = vTextureInfo.back();
					textureInfo.Width = uWidth;
					textureInfo.Height = uHeight;
					textureInfo.Format = nFormat;
					textureInfo.Offset = *it;
				}
			}
		}
		if (!bResult)
		{
			break;
		}
		UString sTextureName = AToU(reinterpret_cast<char*>(a_pBch) + pBchHeader->SectionOffset[FileSectionType::STRING] + pTextureContent->NameOffset);
		for (n32 j = 0; j < static_cast<n32>(vTextureInfo.size()); j++)
		{
			STextureInfo& textureInfo = vTextureInfo[j];
			pvrtexture::CPVRTexture* pPVRTexture = nullptr;
			n32 nSection = FileSectionType::RAW;
			if ((textureInfo.Format == kTextureFormatL4 || textureInfo.Format == kTextureFormatA4) && textureInfo.Offset < pBchHeader->SectionSize[a_nL4A4Section])
			{
				nSection = a_nL4A4Section;
			}
			if (decode(a_pBch + pBchHeader->SectionOffset[nSection] + textureInfo.Offset, textureInfo.Width, textureInfo.Height, textureInfo.Format, &pPVRTexture) == 0)
			{
				UString sPngFileName = m_sDirName + USTR("/") + sTextureName;
				if (vTextureInfo.size() == 1)
				{
					sPngFileName += USTR(".png");
				}
				else
				{
					sPngFileName += Format(USTR("_%d.png"), j);
				}
				FILE* fp = UFopen(sPngFileName.c_str(), USTR("wb"));
				if (fp == nullptr)
				{
					delete pPVRTexture;
					bResult = false;
					break;
				}
				if (m_bVerbose)
				{
					UPrintf(USTR("save: %") PRIUS USTR("\n"), sPngFileName.c_str());
				}
				png_structp pPng = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
				if (pPng == nullptr)
				{
					fclose(fp);
					delete pPVRTexture;
					bResult = false;
					UPrintf(USTR("ERROR: png_create_write_struct error\n\n"));
					break;
				}
				png_infop pInfo = png_create_info_struct(pPng);
				if (pInfo == nullptr)
				{
					png_destroy_write_struct(&pPng, nullptr);
					fclose(fp);
					delete pPVRTexture;
					bResult = false;
					UPrintf(USTR("ERROR: png_create_info_struct error\n\n"));
					break;
				}
				if (setjmp(png_jmpbuf(pPng)) != 0)
				{
					png_destroy_write_struct(&pPng, &pInfo);
					fclose(fp);
					delete pPVRTexture;
					bResult = false;
					UPrintf(USTR("ERROR: setjmp error\n\n"));
					break;
				}
				png_init_io(pPng, fp);
				png_set_IHDR(pPng, pInfo, textureInfo.Width, textureInfo.Height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
				u8* pData = static_cast<u8*>(pPVRTexture->getDataPtr());
				png_bytepp pRowPointers = new png_bytep[textureInfo.Height];
				for (n32 k = 0; k < textureInfo.Height; k++)
				{
					pRowPointers[k] = pData + k * textureInfo.Width * 4;
				}
				png_set_rows(pPng, pInfo, pRowPointers);
				png_write_png(pPng, pInfo, PNG_TRANSFORM_IDENTITY, nullptr);
				png_destroy_write_struct(&pPng, &pInfo);
				delete[] pRowPointers;
				fclose(fp);
				delete pPVRTexture;
			}
			else
			{
				bResult = false;
				UPrintf(USTR("ERROR: decode error\n\n"));
				break;
			}
		}
	}
	return bResult;
}

template<typename FileSectionType>
bool CBch::importFile(u8* a_pBch, n32 a_nL4A4Section)
{
	bool bResult = true;
	SBchHeader<FileSectionType>* pBchHeader = reinterpret_cast<SBchHeader<FileSectionType>*>(a_pBch);
	u8* pContent = a_pBch + pBchHeader->SectionOffset[FileSectionType::CONTENT];
	SPatriciaMap* pPatriciaMap = reinterpret_cast<SPatriciaMap*>(pContent);
	u32* pTextureContentOffset = reinterpret_cast<u32*>(pContent + pPatriciaMap[kContentTypeTexture].ElementsOffset);
	for (n32 i = 0; i < pPatriciaMap[kContentTypeTexture].Dictionary.NumData; i++)
	{
		vector<STextureInfo> vTextureInfo;
		set<u32> sOffset;
		STextureContent* pTextureContent = reinterpret_cast<STextureContent*>(pContent + pTextureContentOffset[i]);
		if (pTextureContent->Format < kTextureFormatRGBA8888 || pTextureContent->Format > kTextureFormatETC1_A4)
		{
			bResult = false;
			UPrintf(USTR("ERROR: unknown format %d\n\n"), pTextureContent->Format);
			break;
		}
		for (n32 j = 0; j < SDW_ARRAY_COUNT(pTextureContent->UnitCmd); j++)
		{
			u16 uWidth = 0;
			u16 uHeight = 0;
			n32 nFormat = -1;
			vector<u32> vOffset;
			u32* pCommand = reinterpret_cast<u32*>(a_pBch + pBchHeader->SectionOffset[FileSectionType::COMMAND] + pTextureContent->UnitCmd[j].ElementsOffset);
			for (u32 k = 0; k < pTextureContent->UnitCmd[j].Count; )
			{
				if (k + 1 >= pTextureContent->UnitCmd[j].Count)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command count error\n\n"));
					break;
				}
				u32 uCommand = pCommand[k + 1];
				u32 uAddr = uCommand & 0xFFFF;
				u32 uSize = (uCommand >> 20 & 0xFF) + 1;
				if (k + static_cast<u32>(Align(uSize + 1, 2)) > pTextureContent->UnitCmd[j].Count)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command size error\n\n"));
					break;
				}
				switch (uAddr)
				{
				case PICA_REG_TEXTURE0_SIZE:
				case PICA_REG_TEXTURE1_SIZE:
				case PICA_REG_TEXTURE2_SIZE:
					uWidth = pCommand[k] >> 16 & 0xFFFF;
					uHeight = pCommand[k] & 0xFFFF;
					break;
				case PICA_REG_TEXTURE0_ADDR1:
				case PICA_REG_TEXTURE0_ADDR2:
				case PICA_REG_TEXTURE0_ADDR3:
				case PICA_REG_TEXTURE0_ADDR4:
				case PICA_REG_TEXTURE0_ADDR5:
				case PICA_REG_TEXTURE0_ADDR6:
				case PICA_REG_TEXTURE1_ADDR:
				case PICA_REG_TEXTURE2_ADDR:
					{
						u32 uOffset = pCommand[k];
						if (sOffset.insert(uOffset).second)
						{
							vOffset.push_back(uOffset);
						}
					}
					break;
				case PICA_REG_TEXTURE0_FORMAT:
				case PICA_REG_TEXTURE1_FORMAT:
				case PICA_REG_TEXTURE2_FORMAT:
					nFormat = pCommand[k];
					if (nFormat != pTextureContent->Format)
					{
						bResult = false;
						UPrintf(USTR("ERROR: command format error\n\n"));
					}
					break;
				default:
					break;
				}
				if (!bResult)
				{
					break;
				}
				k += static_cast<u32>(Align(uSize + 1, 2));
			}
			if (!bResult)
			{
				break;
			}
			if (!vOffset.empty())
			{
				if (uWidth == 0)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command no width\n\n"));
					break;
				}
				if (uHeight == 0)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command no height\n\n"));
					break;
				}
				if (nFormat == -1)
				{
					bResult = false;
					UPrintf(USTR("ERROR: command no format\n\n"));
					break;
				}
				for (vector<u32>::iterator it = vOffset.begin(); it != vOffset.end(); ++it)
				{
					vTextureInfo.push_back(STextureInfo());
					STextureInfo& textureInfo = vTextureInfo.back();
					textureInfo.Width = uWidth;
					textureInfo.Height = uHeight;
					textureInfo.Format = nFormat;
					textureInfo.Offset = *it;
				}
			}
		}
		if (!bResult)
		{
			break;
		}
		UString sTextureName = AToU(reinterpret_cast<char*>(a_pBch) + pBchHeader->SectionOffset[FileSectionType::STRING] + pTextureContent->NameOffset);
		for (n32 j = 0; j < static_cast<n32>(vTextureInfo.size()); j++)
		{
			STextureInfo& textureInfo = vTextureInfo[j];
			UString sPngFileName = m_sDirName + USTR("/") + sTextureName;
			if (vTextureInfo.size() == 1)
			{
				sPngFileName += USTR(".png");
			}
			else
			{
				sPngFileName += Format(USTR("_%d.png"), j);
			}
			FILE* fp = UFopen(sPngFileName.c_str(), USTR("rb"));
			if (fp == nullptr)
			{
				bResult = false;
				break;
			}
			if (m_bVerbose)
			{
				UPrintf(USTR("load: %") PRIUS USTR("\n"), sPngFileName.c_str());
			}
			png_structp pPng = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
			if (pPng == nullptr)
			{
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: png_create_read_struct error\n\n"));
				break;
			}
			png_infop pInfo = png_create_info_struct(pPng);
			if (pInfo == nullptr)
			{
				png_destroy_read_struct(&pPng, nullptr, nullptr);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: png_create_info_struct error\n\n"));
				break;
			}
			png_infop pEndInfo = png_create_info_struct(pPng);
			if (pEndInfo == nullptr)
			{
				png_destroy_read_struct(&pPng, &pInfo, nullptr);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: png_create_info_struct error\n\n"));
				break;
			}
			if (setjmp(png_jmpbuf(pPng)) != 0)
			{
				png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: setjmp error\n\n"));
				break;
			}
			png_init_io(pPng, fp);
			png_read_info(pPng, pInfo);
			n32 nPngWidth = png_get_image_width(pPng, pInfo);
			if (nPngWidth != textureInfo.Width)
			{
				png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: nPngWidth != Width\n\n"));
				break;
			}
			n32 nPngHeight = png_get_image_height(pPng, pInfo);
			if (nPngHeight != textureInfo.Height)
			{
				png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: nPngHeight != Height\n\n"));
				break;
			}
			n32 nBitDepth = png_get_bit_depth(pPng, pInfo);
			if (nBitDepth != 8)
			{
				png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: nBitDepth != 8\n\n"));
				break;
			}
			n32 nColorType = png_get_color_type(pPng, pInfo);
			if (nColorType != PNG_COLOR_TYPE_RGB_ALPHA)
			{
				png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
				fclose(fp);
				bResult = false;
				UPrintf(USTR("ERROR: nColorType != PNG_COLOR_TYPE_RGB_ALPHA\n\n"));
				break;
			}
			u8* pData = new u8[nPngWidth * nPngHeight * 4];
			png_bytepp pRowPointers = new png_bytep[nPngHeight];
			for (n32 j = 0; j < nPngHeight; j++)
			{
				pRowPointers[j] = pData + j * nPngWidth * 4;
			}
			png_read_image(pPng, pRowPointers);
			png_destroy_read_struct(&pPng, &pInfo, &pEndInfo);
			delete[] pRowPointers;
			fclose(fp);
			pvrtexture::CPVRTexture* pPVRTexture = nullptr;
			n32 nSection = FileSectionType::RAW;
			if ((textureInfo.Format == kTextureFormatL4 || textureInfo.Format == kTextureFormatA4) && textureInfo.Offset < pBchHeader->SectionSize[a_nL4A4Section])
			{
				nSection = a_nL4A4Section;
			}
			bool bSame = decode(a_pBch + pBchHeader->SectionOffset[nSection] + textureInfo.Offset, textureInfo.Width, textureInfo.Height, textureInfo.Format, &pPVRTexture) == 0 && memcmp(pPVRTexture->getDataPtr(), pData, textureInfo.Width * textureInfo.Height * 4) == 0;
			delete pPVRTexture;
			if (!bSame)
			{
				u8* pBuffer = nullptr;
				encode(pData, nPngWidth, nPngHeight, textureInfo.Format, pTextureContent->MipmapSize, s_nBPP[textureInfo.Format], &pBuffer);
				n32 nTextureSize = 0;
				for (n32 l = 0; l < pTextureContent->MipmapSize; l++)
				{
					n32 nMipmapHeight = textureInfo.Height >> l;
					n32 nMipmapWidth = textureInfo.Width >> l;
					nTextureSize += nMipmapHeight * nMipmapWidth * s_nBPP[textureInfo.Format] / 8;
				}
				memcpy(a_pBch + pBchHeader->SectionOffset[nSection] + textureInfo.Offset, pBuffer, nTextureSize);
				delete[] pBuffer;
			}
			delete[] pData;
		}
	}
	return bResult;
}

int CBch::decode(u8* a_pBuffer, n32 a_nWidth, n32 a_nHeight, n32 a_nFormat, pvrtexture::CPVRTexture** a_pPVRTexture)
{
	u8* pRGBA = nullptr;
	u8* pAlpha = nullptr;
	switch (a_nFormat)
	{
	case kTextureFormatRGBA8888:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight * 4];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 4; k++)
					{
						pTemp[(i * 64 + j) * 4 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 4 + 3 - k];
					}
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight * 4];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					for (n32 k = 0; k < 4; k++)
					{
						pRGBA[(i * a_nWidth + j) * 4 + k] = pTemp[(((i / 8) * (a_nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 4 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatRGB888:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight * 3];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 3; k++)
					{
						pTemp[(i * 64 + j) * 3 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 3 + 2 - k];
					}
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight * 3];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					for (n32 k = 0; k < 3; k++)
					{
						pRGBA[(i * a_nWidth + j) * 3 + k] = pTemp[(((i / 8) * (a_nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 3 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatRGBA5551:
	case kTextureFormatRGB565:
	case kTextureFormatRGBA4444:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight * 2];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pTemp[(i * 64 + j) * 2 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + k];
					}
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight * 2];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pRGBA[(i * a_nWidth + j) * 2 + k] = pTemp[(((i / 8) * (a_nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatLA88:
	case kTextureFormatHL8:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight * 2];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pTemp[(i * 64 + j) * 2 + k] = a_pBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + 1 - k];
					}
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight * 2];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					for (n32 k = 0; k < 2; k++)
					{
						pRGBA[(i * a_nWidth + j) * 2 + k] = pTemp[(((i / 8) * (a_nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k];
					}
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatL8:
	case kTextureFormatA8:
	case kTextureFormatLA44:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j++)
				{
					pTemp[i * 64 + j] = a_pBuffer[i * 64 + s_nDecodeTransByte[j]];
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					pRGBA[i * a_nWidth + j] = pTemp[((i / 8) * (a_nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8];
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatL4:
	case kTextureFormatA4:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 64; i++)
			{
				for (n32 j = 0; j < 64; j += 2)
				{
					pTemp[i * 64 + j] = (a_pBuffer[i * 32 + s_nDecodeTransByte[j] / 2] & 0xF) * 0x11;
					pTemp[i * 64 + j + 1] = (a_pBuffer[i * 32 + s_nDecodeTransByte[j] / 2] >> 4 & 0xF) * 0x11;
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					pRGBA[i * a_nWidth + j] = pTemp[((i / 8) * (a_nWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8];
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatETC1:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight / 2];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 2 / 8; i++)
			{
				for (n32 j = 0; j < 8; j++)
				{
					pTemp[i * 8 + j] = a_pBuffer[i * 8 + 7 - j];
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight / 2];
			for (n32 i = 0; i < a_nHeight; i += 4)
			{
				for (n32 j = 0; j < a_nWidth; j += 4)
				{
					memcpy(pRGBA + ((i / 4) * (a_nWidth / 4) + j / 4) * 8, pTemp + (((i / 8) * (a_nWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, 8);
				}
			}
			delete[] pTemp;
		}
		break;
	case kTextureFormatETC1_A4:
		{
			u8* pTemp = new u8[a_nWidth * a_nHeight / 2];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 2 / 8; i++)
			{
				for (n32 j = 0; j < 8; j++)
				{
					pTemp[i * 8 + j] = a_pBuffer[8 + i * 16 + 7 - j];
				}
			}
			pRGBA = new u8[a_nWidth * a_nHeight / 2];
			for (n32 i = 0; i < a_nHeight; i += 4)
			{
				for (n32 j = 0; j < a_nWidth; j += 4)
				{
					memcpy(pRGBA + ((i / 4) * (a_nWidth / 4) + j / 4) * 8, pTemp + (((i / 8) * (a_nWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, 8);
				}
			}
			delete[] pTemp;
			pTemp = new u8[a_nWidth * a_nHeight];
			for (n32 i = 0; i < a_nWidth * a_nHeight / 16; i++)
			{
				for (n32 j = 0; j < 4; j++)
				{
					pTemp[i * 16 + j] = (a_pBuffer[i * 16 + j * 2] & 0x0F) * 0x11;
					pTemp[i * 16 + j + 4] = (a_pBuffer[i * 16 + j * 2] >> 4 & 0x0F) * 0x11;
					pTemp[i * 16 + j + 8] = (a_pBuffer[i * 16 + j * 2 + 1] & 0x0F) * 0x11;
					pTemp[i * 16 + j + 12] = (a_pBuffer[i * 16 + j * 2 + 1] >> 4 & 0x0F) * 0x11;
				}
			}
			pAlpha = new u8[a_nWidth * a_nHeight];
			for (n32 i = 0; i < a_nHeight; i++)
			{
				for (n32 j = 0; j < a_nWidth; j++)
				{
					pAlpha[i * a_nWidth + j] = pTemp[(((i / 8) * (a_nWidth / 8) + j / 8) * 4 + i % 8 / 4 * 2 + j % 8 / 4) * 16 + i % 4 * 4 + j % 4];
				}
			}
			delete[] pTemp;
		}
		break;
	}
	PVRTextureHeaderV3 pvrTextureHeaderV3;
	switch (a_nFormat)
	{
	case kTextureFormatRGBA8888:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8).PixelTypeID;
		break;
	case kTextureFormatRGB888:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA5551:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 5, 5, 5, 1).PixelTypeID;
		break;
	case kTextureFormatRGB565:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 5, 6, 5, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA4444:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 4, 4, 4, 4).PixelTypeID;
		break;
	case kTextureFormatLA88:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatHL8:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('r', 'g', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL8:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA8:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatLA44:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 4, 4, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL4:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA4:
		pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatETC1:
		pvrTextureHeaderV3.u64PixelFormat = ePVRTPF_ETC1;
		break;
	case kTextureFormatETC1_A4:
		pvrTextureHeaderV3.u64PixelFormat = ePVRTPF_ETC1;
		break;
	}
	pvrTextureHeaderV3.u32Height = a_nHeight;
	pvrTextureHeaderV3.u32Width = a_nWidth;
	MetaDataBlock metaDataBlock;
	metaDataBlock.DevFOURCC = PVRTEX3_IDENT;
	metaDataBlock.u32Key = ePVRTMetaDataTextureOrientation;
	metaDataBlock.u32DataSize = 3;
	metaDataBlock.Data = new PVRTuint8[metaDataBlock.u32DataSize];
	metaDataBlock.Data[0] = ePVRTOrientRight;
	metaDataBlock.Data[1] = ePVRTOrientUp;
	metaDataBlock.Data[2] = ePVRTOrientIn;
	pvrtexture::CPVRTextureHeader pvrTextureHeader(pvrTextureHeaderV3, 1, &metaDataBlock);
	*a_pPVRTexture = new pvrtexture::CPVRTexture(pvrTextureHeader, pRGBA);
	delete[] pRGBA;
	pvrtexture::Transcode(**a_pPVRTexture, pvrtexture::PVRStandard8PixelType, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB);
	if (a_nFormat == kTextureFormatETC1_A4)
	{
		u8* pData = static_cast<u8*>((*a_pPVRTexture)->getDataPtr());
		for (n32 i = 0; i < a_nHeight; i++)
		{
			for (n32 j = 0; j < a_nWidth; j++)
			{
				pData[(i * a_nWidth + j) * 4 + 3] = pAlpha[i * a_nWidth + j];
			}
		}
		delete[] pAlpha;
	}
	return 0;
}

void CBch::encode(u8* a_pData, n32 a_nWidth, n32 a_nHeight, n32 a_nFormat, n32 a_nMipmapLevel, n32 a_nBPP, u8** a_pBuffer)
{
	PVRTextureHeaderV3 pvrTextureHeaderV3;
	pvrTextureHeaderV3.u64PixelFormat = pvrtexture::PVRStandard8PixelType.PixelTypeID;
	pvrTextureHeaderV3.u32Height = a_nHeight;
	pvrTextureHeaderV3.u32Width = a_nWidth;
	MetaDataBlock metaDataBlock;
	metaDataBlock.DevFOURCC = PVRTEX3_IDENT;
	metaDataBlock.u32Key = ePVRTMetaDataTextureOrientation;
	metaDataBlock.u32DataSize = 3;
	metaDataBlock.Data = new PVRTuint8[metaDataBlock.u32DataSize];
	metaDataBlock.Data[0] = ePVRTOrientRight;
	metaDataBlock.Data[1] = ePVRTOrientUp;
	metaDataBlock.Data[2] = ePVRTOrientIn;
	pvrtexture::CPVRTextureHeader pvrTextureHeader(pvrTextureHeaderV3, 1, &metaDataBlock);
	pvrtexture::CPVRTexture* pPVRTexture = nullptr;
	pvrtexture::CPVRTexture* pPVRTextureAlpha = nullptr;
	if (a_nFormat != kTextureFormatETC1_A4)
	{
		pPVRTexture = new pvrtexture::CPVRTexture(pvrTextureHeader, a_pData);
	}
	else
	{
		u8* pRGBAData = new u8[a_nWidth * a_nHeight * 4];
		memcpy(pRGBAData, a_pData, a_nWidth * a_nHeight * 4);
		u8* pAlphaData = new u8[a_nWidth * a_nHeight * 4];
		memcpy(pAlphaData, a_pData, a_nWidth * a_nHeight * 4);
		for (n32 i = 0; i < a_nWidth * a_nHeight; i++)
		{
			pRGBAData[i * 4 + 3] = 0xFF;
			pAlphaData[i * 4] = 0;
			pAlphaData[i * 4 + 1] = 0;
			pAlphaData[i * 4 + 2] = 0;
		}
		pPVRTexture = new pvrtexture::CPVRTexture(pvrTextureHeader, pRGBAData);
		pPVRTextureAlpha = new pvrtexture::CPVRTexture(pvrTextureHeader, pAlphaData);
		delete[] pRGBAData;
		delete[] pAlphaData;
	}
	if (a_nMipmapLevel != 1)
	{
		pvrtexture::GenerateMIPMaps(*pPVRTexture, pvrtexture::eResizeNearest, a_nMipmapLevel);
		if (a_nFormat == kTextureFormatETC1_A4)
		{
			pvrtexture::GenerateMIPMaps(*pPVRTextureAlpha, pvrtexture::eResizeNearest, a_nMipmapLevel);
		}
	}
	pvrtexture::uint64 uPixelFormat = 0;
	pvrtexture::ECompressorQuality eCompressorQuality = pvrtexture::ePVRTCBest;
	switch (a_nFormat)
	{
	case kTextureFormatRGBA8888:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 8, 8, 8, 8).PixelTypeID;
		break;
	case kTextureFormatRGB888:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 8, 8, 8, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA5551:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 5, 5, 5, 1).PixelTypeID;
		break;
	case kTextureFormatRGB565:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 0, 5, 6, 5, 0).PixelTypeID;
		break;
	case kTextureFormatRGBA4444:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 'b', 'a', 4, 4, 4, 4).PixelTypeID;
		break;
	case kTextureFormatLA88:
		uPixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatHL8:
		uPixelFormat = pvrtexture::PixelType('r', 'g', 0, 0, 8, 8, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL8:
		uPixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA8:
		uPixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatLA44:
		uPixelFormat = pvrtexture::PixelType('l', 'a', 0, 0, 4, 4, 0, 0).PixelTypeID;
		break;
	case kTextureFormatL4:
		uPixelFormat = pvrtexture::PixelType('l', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatA4:
		uPixelFormat = pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID;
		break;
	case kTextureFormatETC1:
		uPixelFormat = ePVRTPF_ETC1;
		eCompressorQuality = pvrtexture::eETCSlowPerceptual;
		break;
	case kTextureFormatETC1_A4:
		uPixelFormat = ePVRTPF_ETC1;
		eCompressorQuality = pvrtexture::eETCSlowPerceptual;
		break;
	}
	pvrtexture::Transcode(*pPVRTexture, uPixelFormat, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB, eCompressorQuality);
	if (a_nFormat == kTextureFormatETC1_A4)
	{
		pvrtexture::Transcode(*pPVRTextureAlpha, pvrtexture::PixelType('a', 0, 0, 0, 8, 0, 0, 0).PixelTypeID, ePVRTVarTypeUnsignedByteNorm, ePVRTCSpacelRGB, pvrtexture::ePVRTCBest);
	}
	n32 nTotalSize = 0;
	n32 nCurrentSize = 0;
	for (n32 l = 0; l < a_nMipmapLevel; l++)
	{
		nTotalSize += (a_nWidth >> l) * (a_nHeight >> l) * a_nBPP / 8;
	}
	*a_pBuffer = new u8[nTotalSize];
	for (n32 l = 0; l < a_nMipmapLevel; l++)
	{
		n32 nMipmapWidth = a_nWidth >> l;
		n32 nMipmapHeight = a_nHeight >> l;
		u8* pRGBA = static_cast<u8*>(pPVRTexture->getDataPtr(l));
		u8* pAlpha = nullptr;
		if (a_nFormat == kTextureFormatETC1_A4)
		{
			pAlpha = static_cast<u8*>(pPVRTextureAlpha->getDataPtr(l));
		}
		switch (a_nFormat)
		{
		case kTextureFormatRGBA8888:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 4];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 4; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 4 + k] = pRGBA[(i * nMipmapWidth + j) * 4 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 4; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 4 + 3 - k] = pTemp[(i * 64 + j) * 4 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatRGB888:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 3];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 3; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 3 + k] = pRGBA[(i * nMipmapWidth + j) * 3 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 3; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 3 + 2 - k] = pTemp[(i * 64 + j) * 3 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatRGBA5551:
		case kTextureFormatRGB565:
		case kTextureFormatRGBA4444:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 2];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k] = pRGBA[(i * nMipmapWidth + j) * 2 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + k] = pTemp[(i * 64 + j) * 2 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatLA88:
		case kTextureFormatHL8:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight * 2];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8) * 2 + k] = pRGBA[(i * nMipmapWidth + j) * 2 + k];
						}
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						for (n32 k = 0; k < 2; k++)
						{
							pMipmapBuffer[(i * 64 + s_nDecodeTransByte[j]) * 2 + 1 - k] = pTemp[(i * 64 + j) * 2 + k];
						}
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatL8:
		case kTextureFormatA8:
		case kTextureFormatLA44:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						pTemp[((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8] = pRGBA[i * nMipmapWidth + j];
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j++)
					{
						pMipmapBuffer[i * 64 + s_nDecodeTransByte[j]] = pTemp[i * 64 + j];
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatL4:
		case kTextureFormatA4:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						pTemp[((i / 8) * (nMipmapWidth / 8) + j / 8) * 64 + i % 8 * 8 + j % 8] = pRGBA[i * nMipmapWidth + j];
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 64; i++)
				{
					for (n32 j = 0; j < 64; j += 2)
					{
						pMipmapBuffer[i * 32 + s_nDecodeTransByte[j] / 2] = ((pTemp[i * 64 + j] / 0x11) & 0x0F) | ((pTemp[i * 64 + j + 1] / 0x11) << 4 & 0xF0);
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatETC1:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight / 2];
				for (n32 i = 0; i < nMipmapHeight; i += 4)
				{
					for (n32 j = 0; j < nMipmapWidth; j += 4)
					{
						memcpy(pTemp + (((i / 8) * (nMipmapWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, pRGBA + ((i / 4) * (nMipmapWidth / 4) + j / 4) * 8, 8);
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 2 / 8; i++)
				{
					for (n32 j = 0; j < 8; j++)
					{
						pMipmapBuffer[i * 8 + 7 - j] = pTemp[i * 8 + j];
					}
				}
				delete[] pTemp;
			}
			break;
		case kTextureFormatETC1_A4:
			{
				u8* pTemp = new u8[nMipmapWidth * nMipmapHeight / 2];
				for (n32 i = 0; i < nMipmapHeight; i += 4)
				{
					for (n32 j = 0; j < nMipmapWidth; j += 4)
					{
						memcpy(pTemp + (((i / 8) * (nMipmapWidth / 8) + j / 8) * 4 + (i % 8 / 4 * 2 + j % 8 / 4)) * 8, pRGBA + ((i / 4) * (nMipmapWidth / 4) + j / 4) * 8, 8);
					}
				}
				u8* pMipmapBuffer = *a_pBuffer + nCurrentSize;
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 2 / 8; i++)
				{
					for (n32 j = 0; j < 8; j++)
					{
						pMipmapBuffer[8 + i * 16 + 7 - j] = pTemp[i * 8 + j];
					}
				}
				delete[] pTemp;
				pTemp = new u8[nMipmapWidth * nMipmapHeight];
				for (n32 i = 0; i < nMipmapHeight; i++)
				{
					for (n32 j = 0; j < nMipmapWidth; j++)
					{
						pTemp[(((i / 8) * (nMipmapWidth / 8) + j / 8) * 4 + i % 8 / 4 * 2 + j % 8 / 4) * 16 + i % 4 * 4 + j % 4] = pAlpha[i * nMipmapWidth + j];
					}
				}
				for (n32 i = 0; i < nMipmapWidth * nMipmapHeight / 16; i++)
				{
					for (n32 j = 0; j < 4; j++)
					{
						pMipmapBuffer[i * 16 + j * 2] = ((pTemp[i * 16 + j] / 0x11) & 0x0F) | ((pTemp[i * 16 + j + 4] / 0x11) << 4 & 0xF0);
						pMipmapBuffer[i * 16 + j * 2 + 1] = ((pTemp[i * 16 + j + 8] / 0x11) & 0x0F) | ((pTemp[i * 16 + j + 12] / 0x11) << 4 & 0xF0);
					}
				}
				delete[] pTemp;
			}
			break;
		}
		nCurrentSize += nMipmapWidth * nMipmapHeight * a_nBPP / 8;
	}
	delete pPVRTexture;
	if (a_nFormat == kTextureFormatETC1_A4)
	{
		delete pPVRTextureAlpha;
	}
}
