#ifndef BCH_H_
#define BCH_H_

#include <sdw.h>

namespace pvrtexture
{
	class CPVRTexture;
}

enum EContentType
{
	kContentTypeModel,
	kContentTypeMaterial,
	kContentTypeShader,
	kContentTypeTexture,
	kContentTypeLutSet,
	kContentTypeLight,
	kContentTypeCamera,
	kContentTypeFog,
	kContentTypeSkeletalAnim,
	kContentTypeMaterialAnim,
	kContentTypeVisibilityAnim,
	kContentTypeLightAnim,
	kContentTypeCameraAnim,
	kContentTypeFogAnim,
	kContentTypeSceneEnvironment
};

enum PicaReg
{
	PICA_REG_TEXTURE0_SIZE = 0x082,
	PICA_REG_TEXTURE0_ADDR1 = 0x085,
	PICA_REG_TEXTURE0_ADDR2 = 0x086,
	PICA_REG_TEXTURE0_ADDR3 = 0x087,
	PICA_REG_TEXTURE0_ADDR4 = 0x088,
	PICA_REG_TEXTURE0_ADDR5 = 0x089,
	PICA_REG_TEXTURE0_ADDR6 = 0x08a,
	PICA_REG_TEXTURE0_FORMAT = 0x08e,
	PICA_REG_TEXTURE1_SIZE = 0x092,
	PICA_REG_TEXTURE1_ADDR = 0x095,
	PICA_REG_TEXTURE1_FORMAT = 0x096,
	PICA_REG_TEXTURE2_SIZE = 0x09a,
	PICA_REG_TEXTURE2_ADDR = 0x09d,
	PICA_REG_TEXTURE2_FORMAT = 0x09e,
};

struct SFileSectionType0
{
	enum enum_t
	{
		CONTENT,
		STRING,
		COMMAND,
		RAW,
		RELOCATABLE_TABLE,
		QUANTITY,
		UNINIT_DATA = QUANTITY,
		UNINIT_COMMAND,
		FULL_QUANTITY
	};
};

struct SFileSectionType1
{
	enum enum_t
	{
		CONTENT,
		STRING,
		COMMAND,
		RAW,
		RAW_EXT,
		RELOCATABLE_TABLE,
		QUANTITY,
		UNINIT_DATA = QUANTITY,
		UNINIT_COMMAND,
		FULL_QUANTITY
	};
};

#include SDW_MSC_PUSH_PACKED
template<typename FileSectionType>
struct SBchHeader
{
	u32 Signature;
	u8 BackwardCompatibility;
	u8 ForwardCompatibility;
	u16 Revision;
	u32 SectionOffset[FileSectionType::QUANTITY];
	u32 SectionSize[FileSectionType::FULL_QUANTITY];
	u8 Flags;
	u8 Padding;
	u16 PhysicalAddressCount;
} SDW_GNUC_PACKED;

struct SPatriciaDic
{
	u16 NumData;
	u16 Padding;
	u32 NodesOffset;
} SDW_GNUC_PACKED;

struct SPatriciaMap
{
	u32 ElementsOffset;
	SPatriciaDic Dictionary;
} SDW_GNUC_PACKED;

struct SCommand
{
	u32 ElementsOffset;
	u32 Count;
} SDW_GNUC_PACKED;

struct STextureContent
{
	SCommand UnitCmd[3];
	u8 Format;
	u8 MipmapSize;
	u8 Padding0[2];
	u32 NameOffset;
} SDW_GNUC_PACKED;
#include SDW_MSC_POP_PACKED

struct STextureInfo
{
	u16 Width;
	u16 Height;
	u32 Format;
	u32 Offset;
};

class CBch
{
public:
	enum ETextureFormat
	{
		kTextureFormatRGBA8888 = 0,
		kTextureFormatRGB888 = 1,
		kTextureFormatRGBA5551 = 2,
		kTextureFormatRGB565 = 3,
		kTextureFormatRGBA4444 = 4,
		kTextureFormatLA88 = 5,
		kTextureFormatHL8 = 6,
		kTextureFormatL8 = 7,
		kTextureFormatA8 = 8,
		kTextureFormatLA44 = 9,
		kTextureFormatL4 = 10,
		kTextureFormatA4 = 11,
		kTextureFormatETC1 = 12,
		kTextureFormatETC1_A4 = 13
	};
	CBch();
	~CBch();
	void SetFileName(const UString& a_sFileName);
	void SetDirName(const UString& a_sDirName);
	void SetVerbose(bool a_bVerbose);
	bool ExportFile();
	bool ImportFile();
	static bool IsBchFile(const UString& a_sFileName);
	static const u32 s_uSignature;
	static const int s_nBPP[];
	static const int s_nDecodeTransByte[64];
private:
	template<typename FileSectionType>
	bool exportFile(u8* a_pBch, n32 a_nL4A4Section);
	template<typename FileSectionType>
	bool importFile(u8* a_pBch, n32 a_nL4A4Section);
	static int decode(u8* a_pBuffer, n32 a_nWidth, n32 a_nHeight, n32 a_nFormat, pvrtexture::CPVRTexture** a_pPVRTexture);
	static void encode(u8* a_pData, n32 a_nWidth, n32 a_nHeight, n32 a_nFormat, n32 a_nMipmapLevel, n32 a_nBPP, u8** a_pBuffer);
	UString m_sFileName;
	UString m_sDirName;
	bool m_bVerbose;
};

#endif	// BCH_H_
