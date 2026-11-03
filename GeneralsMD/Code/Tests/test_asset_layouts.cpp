#include "test_harness.h"

#include "bittype.h"
#include "chunkio.h"
#include "TARGA.H"
#include "ddsfile.h"

#include <cstring>
#include <cstdint>
#include <cstddef>

TEST(lp64_file_integers_stay_32_bit) {
	CHECK_EQ(sizeof(uint32), 4u);
	CHECK_EQ(sizeof(sint32), 4u);
	struct TwoInts {
		uint32 a;
		uint32 b;
	};
	CHECK_EQ(sizeof(TwoInts), 8u);
}

TEST(w3d_chunk_header_reads_two_uint32s) {
	uint8_t image[8];
	const uint32_t chunk_type = 0x00000001u;
	const uint32_t chunk_size = 12u;
	std::memcpy(&image[0], &chunk_type, sizeof(chunk_type));
	std::memcpy(&image[4], &chunk_size, sizeof(chunk_size));

	CHECK_EQ(sizeof(ChunkHeader), 8u);

	ChunkHeader header;
	std::memcpy(&header, image, sizeof(header));
	CHECK_EQ(header.Get_Type(), 1u);
	CHECK_EQ(header.Get_Size(), 12u);
}

TEST(tga2_footer_signature_sits_at_the_seek_the_loader_uses) {
	uint8_t image[26];
	std::memset(image, 0, sizeof(image));

	const int32_t extension = 18;
	const int32_t developer = 0;
	std::memcpy(&image[0], &extension, sizeof(extension));
	std::memcpy(&image[4], &developer, sizeof(developer));
	std::memcpy(&image[8], TGA2_SIGNATURE, 16);
	image[24] = '.';
	image[25] = 0;

	CHECK_EQ(sizeof(TGA2Footer), 26u);

	TGA2Footer footer;
	std::memcpy(&footer, image, sizeof(footer));
	CHECK(std::strncmp(footer.Signature, TGA2_SIGNATURE, 16) == 0);
	CHECK_EQ(footer.RsvdChar, '.');
	CHECK_EQ(footer.Extension, 18);
}

TEST(tga2_extension_read_is_495_bytes) {
	CHECK_EQ(sizeof(TGA2Extension), 495u);

	uint8_t image[495];
	std::memset(image, 0, sizeof(image));
	const uint16_t ext_size = 495u;
	std::memcpy(&image[0], &ext_size, sizeof(ext_size));

	TGA2Extension ext;
	std::memcpy(&ext, image, sizeof(ext));
	CHECK_EQ(ext.ExtSize, 495);
}

TEST(dds_desc_overlay_matches_the_loader_size_check) {
	uint8_t image[124];
	std::memset(image, 0, sizeof(image));

	const uint32_t size = 124u;
	const uint32_t height = 64u;
	const uint32_t width = 128u;
	const uint32_t fourcc = 0x31545844u; // DXT1

	std::memcpy(&image[offsetof(LegacyDDSURFACEDESC2, Size)], &size, sizeof(size));
	std::memcpy(&image[offsetof(LegacyDDSURFACEDESC2, Height)], &height, sizeof(height));
	std::memcpy(&image[offsetof(LegacyDDSURFACEDESC2, Width)], &width, sizeof(width));
	std::memcpy(&image[offsetof(LegacyDDSURFACEDESC2, PixelFormat) + offsetof(LegacyDDPIXELFORMAT, FourCC)],
	            &fourcc, sizeof(fourcc));

	LegacyDDSURFACEDESC2 desc;
	CHECK_EQ(sizeof(LegacyDDSURFACEDESC2), 124u);
	CHECK_EQ(sizeof(desc.Surface), 4u);

	std::memcpy(&desc, image, sizeof(desc));
	CHECK_EQ(desc.Size, 124u);
	CHECK_EQ(desc.Height, 64u);
	CHECK_EQ(desc.Width, 128u);
	CHECK_EQ(desc.PixelFormat.FourCC, 0x31545844u);
}
