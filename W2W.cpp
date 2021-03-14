/*
WRITE TO WOZ (W2W)
A command-line tool to directly write binary to a WOZ image.
(heavely based on DSK2WOZ (c) 2018 Thomas Harte)

Copyright (c) 2021 - GROUiK/FRENCH TOUCH / Thomas Harte
MIT License

v0.31 - Custom 32 sectors/128 bytes - with GAPS custom (GAP1 = 8 / GAP2 = 7 / GAP3 = 8)

Usage:
W2W s d track sector image.woz binary.b [-v]
[s]: standard track(s) / [c]: custom track(s)
interleaving: [d] dos / [p]: physical / [i1]: custom1
first [track] number
first [sector] number
image.woz name
binary.b name
-v verbose mode (optional)
*/

#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// Forward declarations; see definitions for documentation.
static uint32_t crc32(const uint8_t* buf, size_t size);
static void serialise_sector_standard(uint8_t* dest, const uint8_t* src, size_t track_position, size_t sector, size_t track_number, bool bVerbose);
static void serialise_sector_custom1(uint8_t* dest, const uint8_t* src, size_t track_position, size_t sector, size_t track_number, bool bVerbose);


int main(int argc, char* argv[]) {
	// Announce failure if there are anything other than six arguments.
	if (argc != 7 && argc !=8) {
		// args#        0  1 2    3      4         5       6	   7
		printf("USAGE: W2W s d track# sector# image.woz binary.b [-v]\n");
		return -1;
	}
	// Retrieving and testing arguments:
	bool bVerbose = 0;  // default
	if (argc == 8) {
		if ((strcmp(argv[7], "-v") == 0) || (strcmp(argv[7], "-V") == 0)) {			// interleaving physical
			bVerbose = 1;
		}
	}

	// structure type (standard or custom)
	bool bStructure;
	uint32_t sectors_per_track;
	uint32_t sector_size;
	if ((strcmp(argv[1], "c") == 0) || (strcmp(argv[1], "C") == 0)) {			// custom1
		bStructure = 1;
		sector_size = 128;
		sectors_per_track = 32;
	}
	else {
		bStructure = 0;
		sector_size = 256;							// standard
		sectors_per_track = 16;
	}
	uint8_t interleaving = 0;													// interleaving dos (default)
	if ((strcmp(argv[2], "p") == 0) || (strcmp(argv[2], "P") == 0)) {			// interleaving physical
		interleaving = 1;
	}
	else if
		((strcmp(argv[2], "i1") == 0) || (strcmp(argv[2], "I1") == 0)) {		// custom interleaving 1
		interleaving = 2;
	}

	// track
	uint32_t first_track = strtol(argv[3], NULL, 0); // prefix 0x or 0X for hexa, no prefix for decimal!
	// sector
	uint32_t first_sector = strtol(argv[4], NULL, 0); // prefix 0x or 0X for hexa, no prefix for decimal!

	// Attempt to open WOZ file (read/write)
	FILE* const woz_file = fopen(argv[5], "r+b");
	if (!woz_file) {
		printf("ERROR: could not open %s\n", argv[5]);
		return -2;
	}
	const size_t woz_image_size = 256 + 35 * 6656;								// 233216 bytes - WOZ1
	uint8_t woz[woz_image_size];
	// reading WOZ
	const size_t woz_bytes_read = fread(woz, 1, woz_image_size, woz_file);
	fseek(woz_file, 0, SEEK_SET);												// back to the beginning of the file

	// Attempt to open binary file (read)
	FILE* const binary_file = fopen(argv[6], "rb");
	if (!binary_file) {
		printf("ERROR: could not open %s for reading\n", argv[6]);
		return -2;
	}
	// reading binary
	fseek(binary_file, 0, SEEK_END);											// seek to end of file
	const size_t binary_image_size = ftell(binary_file);						// get current file pointer
	fseek(binary_file, 0, SEEK_SET);											// seek back to beginning of file

	// calc nb of sectors to write to the WOZ image
	size_t nb_sectors = 0;
	if (binary_image_size % sector_size)										// if modulo !=0
		nb_sectors = (binary_image_size / sector_size) + 1;
	else
		nb_sectors = (binary_image_size / sector_size);

	unsigned char* binary = (unsigned char*)calloc(nb_sectors * sector_size, sizeof (unsigned char));	// allocate memory
	if (binary) {
		const size_t binary_bytes_read = fread(binary, 1, binary_image_size, binary_file);
		fclose(binary_file);
	}
	else {
		printf("ERROR: could not allocate memory for buffer");
		fclose(binary_file);
		return -2;
	}


	/*
	So at this point:
	- we know how many sectors - rounded up to the upper #sector completed with 0 - to write to the woz file.
	- we know where to begin (track/sector) in the woz file
	- we know which structure (standard/custom) to use
	*/

	// ======================================================================================== //
	// offsets of each sector header for one track 

	const int Offset_Standard_Header[] = {
		   //00  01    02   03   04    05    06    07    08    09    10    11    12   13     14   15  
			160,3294,6428,9562,12696,15830,18964,22098,25232,28366,31500,34634,37768,40902,44036,47170
	};
	/* - GAPS (GAP1 = 5 / GAP2 = 5 / GAP3 = 5)
	const int Offset_Custom1_Header[] = {
		  //00  01    02  03   04    05  06    07    08    09    10    11    12   13     14   15  
			50,1590,3130,4670,6210,7750,9290,10830,12370,13910,15450,16990,18530,20070,21610,23150,
		  //  16    17    18    19    20    21    22    23    24    25    26    27    28    29    30   31    32     33
			24690,26230,27770,29310,30850,32390,33930,35470,37010,38550,40090,41630,43170,44710,46250,47790,49330,50870
	};*/
	/* - GAPS (GAP1 = 8 / GAP2 = 7 / GAP3 = 8)*/
	const int Offset_Custom1_Header[] = {
		//00  01    02  03   04    05  06    07    08    09    10    11    12   13     14   15  
		  80,1670,3260,4850,6440,8030,9620,11210,12800,14390,15980,17570,19160,20750,22340,23930,
		  //  16    17    18    19    20    21    22    23    24    25    26    27    28    29    30   31
			25520,27110,28700,30290,31880,33470,35060,36650,38240,39830,41420,43010,44600,46190,47780,49370
	};
	// Interleavings for Standard Structures:
	const int Standard_Dos_Interleaving[] = {
			0x00,0x0D,0x0B,0x09,0x07,0x05,0x03,0x01,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0x0F
	};
	const int Standard_Physical_Interleaving[] = {
			0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F
	};
	const int Standard_Interleaving1[] = {
			0x00,0x0D,0x0B,0x09,0x07,0x05,0x03,0x01,0x0E,0x0C,0x0A,0x08,0x06,0x04,0x02,0x0F
	}; // same as DOS for now - not needed actually
	
	// Interleavings for Custom Structures:	
	const int Custom_Dos_Interleaving[] = {
			0x00,0x10,0x01,0x11,0x02,0x12,0x03,0x13,0x04,0x14,0x05,0x15,0x06,0x16,0x07,0x17,
			0x08,0x18,0x09,0x19,0x0A,0x1A,0x0B,0x1B,0x0C,0x1C,0x0D,0x1D,0x0E,0x1E,0x0F,0x1F
	};
	const int Custom_Physical_Interleaving[] = {
			0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
			0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F
	};
	const int Custom_Interleaving1[] = {
			0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23, 8, 9, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29, 30, 31
	};

	// Write the DATA
	size_t offset_header;
	size_t track = first_track;				// init track
	size_t sector = first_sector;			// init sector 
	uint8_t* src;
	uint8_t* dest;
	size_t physical_sector;

	for (size_t j=0; j < nb_sectors; j++) {

		// standard
		if (!bStructure) {		
			// interleaving
			if (interleaving == 1) {
				physical_sector = Standard_Physical_Interleaving[sector];		// interleaving physical
			}
			else if (interleaving == 2) {
				physical_sector = Standard_Interleaving1[sector];				// interleaving custom 1
			}
			else {			// interleaving = 0 (or other)
				physical_sector = Standard_Dos_Interleaving[sector];			// interleaving dos (default)
			}
			offset_header = Offset_Standard_Header[physical_sector];
		}
		// custom structure
		else {			
			// interleaving
			if (interleaving == 1) {
				physical_sector = Custom_Physical_Interleaving[sector];			// interleaving physical
			}
			else if (interleaving == 2) {
				physical_sector = Custom_Interleaving1[sector];					// interleaving custom 1
			}
			else {			// interleaving = 0 (or others)
				physical_sector = Custom_Dos_Interleaving[sector];				// interleaving dos (default)
			}
			offset_header = Offset_Custom1_Header[physical_sector];
		}

		dest = woz + 256 + (track * 6656);										// offset of the concerned track in the WOZ image buffer
		src = binary + (j * sector_size);										// offset in binary buffer

		if (!bStructure) {
			serialise_sector_standard(dest, src, offset_header, physical_sector, track, bVerbose);
		}
		else {
			serialise_sector_custom1(dest, src, offset_header, physical_sector, track, bVerbose);
		}

		// prepare next sector
		sector++;
		if (sector > (sectors_per_track - 1)) {
			sector = 0;					// reinit first sector for next track
			track++;					// next track
		}
	}
	// ======================================================================================== //

	const uint32_t crc = crc32(woz+12, sizeof(woz)-12);
	woz[8] = crc & 0xff;
	woz[9] = (crc >> 8) & 0xff;
	woz[10] = (crc >> 16) & 0xff;
	woz[11] = (crc >> 24);

	if (sizeof(woz) != 233216) {
		printf("ERROR: Bad WOZ Buffer size. Image file was not modified!\n");
		fclose(woz_file);
		return -6;
	}

	const size_t length_written = fwrite(woz, 1, sizeof(woz), woz_file);
	fclose(woz_file);

	if (length_written != sizeof(woz)) {
		printf("ERROR: Could not write full WOZ image\n");
		return -6;
	}

	return 0;
}

/*
	CRC generator. Essentially that of Gary S. Brown from 1986, but I've
	fixed the initial value. This is exactly the code advocated by the
	WOZ file specifications (with some extra consts).
*/
static const uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/*!
	Computes the CRC32 of an input buffer.

	@param buf A pointer to the data to compute a CRC32 from.
	@param size The size of the data to compute a CRC32 from.
	@return The computed CRC32.
*/
static uint32_t crc32(const uint8_t* buf, size_t size) {
	uint32_t crc = ~0;
	size_t byte = 0;
	while (size--) {
		crc = crc32_tab[(crc ^ buf[byte]) & 0xFF] ^ (crc >> 8);
		++byte;
	}
	return ~crc;
}



/*
	Constructs the 6-and-2 DOS 3.3-style on-disk
	representation of a DOS logical-order sector dump.
*/

/*!
	Appends a bit to a buffer at a supplied position, returning the
	position immediately after the bit. The first bit added to a buffer
	will be stored in the MSB of the first byte. The second will be stored in
	bit 6. The eighth will be stored in the MSB of the second byte. Etc.

	@param buffer The buffer to write into.
	@param position The position to write at.
	@param value An indicator of the bit to write. If this is zero then a 0 is written; otherwise a 1 is written.
	@return The position immediately after the bit.
*/
static size_t write_bit(uint8_t* buffer, size_t position, int value) {
	buffer[position >> 3] |= (value ? 0x80 : 0x00) >> (position & 7);
	return position + 1;
}

/*!
	Appends a byte to a buffer at a supplied position, returning the
	position immediately after the byte. This is equivalent to calling
	write_bit eight times, supplying bit 7, then bit 6, down to bit 0.

	@param buffer The buffer to write into.
	@param position The position to write at.
	@param value The byte to write.
	@return The position immediately after the byte.
*/
static size_t write_byte(uint8_t* buffer, size_t position, size_t value) {
	const size_t shift = position & 7;
	const size_t byte_position = position >> 3;

	buffer[byte_position] |= value >> shift;
	if (shift) buffer[byte_position + 1] |= value << (8 - shift);

	return position + 8;
}

static size_t write_byte_prologue(uint8_t* buffer, size_t position, size_t value) {
	const size_t shift = position & 7;
	const size_t byte_position = position >> 3;

	buffer[byte_position] |= value >> shift;
	if (shift) buffer[byte_position + 1] |= value << (8 - shift);
	
	// dirty fix
	if (buffer[byte_position] == 0x0D)
				buffer[byte_position] = 0xCD;
	else if (buffer[byte_position] == 0x03)
				buffer[byte_position] = 0xF3;
	// ====

	return position + 8;
}

/*!
	Encodes a byte into Apple 4-and-4 format and appends it to a buffer.

	@param buffer The buffer to write into.
	@param position The position to write at.
	@param value The byte to encode and write.
	@return The position immediately after the encoded byte.
*/
static size_t write_4_and_4(uint8_t* buffer, size_t position, size_t value) {
	position = write_byte(buffer, position, (value >> 1) | 0xaa);
	position = write_byte(buffer, position, value | 0xaa);
	return position;
}

/*!
	Appends a 6-and-2-style sync word to a buffer.

	@param buffer The buffer to write into.
	@param position The position to write at.
	@return The position immediately after the sync word.
*/
static size_t write_sync(uint8_t* buffer, size_t position) {
	position = write_byte(buffer, position, 0xff);
	return position + 2; // Skip two bits, i.e. leave them as 0s.
}

/*!
	Converts a 256-byte source buffer into the 343 byte values that
	contain the Apple 6-and-2 encoding of that buffer.

	@param dest The at-least-343 byte buffer to which the encoded sector is written.
	@param src The 256-byte source data.
*/
static void encode_6_and_2_256(uint8_t* dest, const uint8_t* src) {
	const uint8_t six_and_two_mapping[] = {
		0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
		0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
		0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
		0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
		0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
		0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
		0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
		0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};

	// Fill in byte values: the first 86 bytes contain shuffled
	// and combined copies of the bottom two bits of the sector
	// contents; the 256 bytes afterwards are the remaining
	// six bits.
	const uint8_t bit_reverse[] = { 0, 2, 1, 3 };
	for (size_t c = 0; c < 84; ++c) {
		dest[c] =
			bit_reverse[src[c] & 3] |
			(bit_reverse[src[c + 86] & 3] << 2) |
			(bit_reverse[src[c + 172] & 3] << 4);
	}
	dest[84] =
		(bit_reverse[src[84] & 3] << 0) |
		(bit_reverse[src[170] & 3] << 2);
	dest[85] =
		(bit_reverse[src[85] & 3] << 0) |
		(bit_reverse[src[171] & 3] << 2);

	for (size_t c = 0; c < 256; ++c) {
		dest[86 + c] = src[c] >> 2;
	}

	// Exclusive OR each byte with the one before it.
	dest[342] = dest[341];
	size_t location = 342;
	while (location > 1) {
		--location;
		dest[location] ^= dest[location - 1];
	}

	// Map six-bit values up to full bytes.
	for (size_t c = 0; c < 343; ++c) {
		dest[c] = six_and_two_mapping[dest[c]];
	}
}


/*!
	Converts a 128-byte source buffer into the xxx byte values that
	contain the Apple 6-and-2 encoding of that buffer.

	@param dest The at-least-172 byte buffer to which the encoded sector is written.
	@param src The 128-byte source data.
*/
static void encode_6_and_2_128(uint8_t* dest, const uint8_t* src) {
	const uint8_t six_and_two_mapping[] = {
		0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
		0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
		0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
		0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
		0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
		0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
		0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
		0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};

	// Fill in byte values: the first 43 bytes contain shuffled
	// and combined copies of the bottom two bits of the "semi" sector
	// contents; the 128 bytes afterwards are the remaining
	// six bits.
	const uint8_t bit_reverse[] = { 0, 2, 1, 3 };
	for (size_t c = 0; c < 42; ++c) {
		dest[c] =
			 bit_reverse[src[c] & 3] |
			(bit_reverse[src[c + 43] & 3] << 2) |
			(bit_reverse[src[c + 86] & 3] << 4);
	}

	dest[42] =
		(bit_reverse[src[42] & 3]) |
		(bit_reverse[src[85] & 3] << 2);

	for (size_t c = 0; c < 128; ++c) {
		dest[43 + c] = src[c] >> 2;
	}

	// Exclusive OR each byte with the one before it.
	dest[171] = dest[170];
	size_t location = 171;
	while (location > 1) {
		--location;
		dest[location] ^= dest[location - 1];
	}

	// Map six-bit values up to full bytes.
	for (size_t c = 0; c < 172; ++c) {
		dest[c] = six_and_two_mapping[dest[c]];
	}
}


/*!
	Write a sector in a standard Track (16 sectors x 256 bytes)

	@param dest: position of the beginning of the track in the woz image file buffer
	@param src: position of the current sector in the binary file buffer
	@param track_position: position of the beginning (header prologue) where to write in the current track buffer 
	@param sector_number
	@param track_number 
	@param interleaving (00: DOS order)
	@param bVerbose (00: off/ 01 on)
*/
static void serialise_sector_standard(uint8_t* dest, const uint8_t* src, size_t track_position, size_t sector_number, size_t track_number, bool bVerbose) {
	memset(dest + (track_position >> 3), 0, (3134>>3));			// init data of this sector with 00
	/*
		Write the sector header.
	*/
	
	// Prologue.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Header Prologue: 0x%llX", track_number, sector_number, track_position >> 3);
	track_position = write_byte_prologue(dest, track_position, 0xd5);
	track_position = write_byte_prologue(dest, track_position, 0xaa);
	track_position = write_byte_prologue(dest, track_position, 0x96);
	if (bVerbose) printf(" - 0x%llX \n", track_position>>3);
	
	// Volume, track, setor and checksum, all in 4-and-4 format.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Header Infos: 0x%llX", track_number, sector_number, track_position>>3);
	track_position = write_4_and_4(dest, track_position, 254);
	track_position = write_4_and_4(dest, track_position, track_number);
	track_position = write_4_and_4(dest, track_position, sector_number);
	track_position = write_4_and_4(dest, track_position, 254 ^ track_number ^ sector_number);
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
	
	// Epilogue.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Header Epilogue: 0x%llX", track_number, sector_number, track_position>>3);
	track_position = write_byte(dest, track_position, 0xde);
	track_position = write_byte(dest, track_position, 0xaa);
	track_position = write_byte(dest, track_position, 0xeb);
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
	
	// Write gap 2.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Gap2: 0x%llX", track_number, sector_number, track_position >> 3);
	for (size_t c = 0; c < 7; ++c) {
		track_position = write_sync(dest, track_position);
	}
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
	
	/*
		Write the sector body.
	*/
	
	// Prologue.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Data Prologue: 0x%llX", track_number, sector_number, track_position >> 3);
	track_position = write_byte(dest, track_position, 0xd5);
	track_position = write_byte(dest, track_position, 0xaa);
	track_position = write_byte(dest, track_position, 0xad);
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
	
	// Sector contents.
	uint8_t contents[343];
	encode_6_and_2_256(contents, src);
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Data Contents: 0x%llX", track_number, sector_number, track_position >> 3);
	for (size_t c = 0; c < sizeof(contents); ++c) {
		track_position = write_byte(dest, track_position, contents[c]);
	}
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
	
	// Epilogue.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Data Epilogue: 0x%llX", track_number, sector_number, track_position >> 3);
	track_position = write_byte(dest, track_position, 0xde);
	track_position = write_byte(dest, track_position, 0xaa);
	track_position = write_byte(dest, track_position, 0xeb);
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
	
	// Write gap 3.
	if (bVerbose) printf("Track %llu (s) / Sector %llu - Gap3: 0x%llX", track_number, sector_number, track_position >> 3);
	for (size_t c = 0; c < 16; ++c) {
		track_position = write_sync(dest, track_position);
	}
	if (bVerbose) printf(" - 0x%llX \n", track_position >> 3);
}

/*!
	Write a sector in a custom Track (32 sectors x 128 bytes)
	- with limited sector informations (sector# only)
	- with limited gaps size

	@param dest: position of the beginning of the track in the woz image file buffer
	@param src: position of the current sector in the binary file buffer
	@param track_position: position of the beginning (header prologue) where to write in the current track buffer 
	@param sector_number
	@param track_number 
	@param interleaving (00: DOS order)
	@param bVerbose (00: off/ 01 on)
*/

static void serialise_sector_custom1(uint8_t* dest, const uint8_t* src, size_t track_position, size_t sector_number, size_t track_number, bool bVerbose) {
	memset(dest + (track_position >> 3), 0, (1590>>3));		// init data of this sector with 00
	/*
		Write the sector header.
	*/

	// Prologue.
	if (bVerbose) printf(" Track %llu (c) / Sector %llu - Header Prologue: %llu", track_number, sector_number, track_position);
	track_position = write_byte_prologue(dest, track_position, 0xd5);
	track_position = write_byte_prologue(dest, track_position, 0xaa);
	track_position = write_byte_prologue(dest, track_position, 0x96);
	if (bVerbose) printf(" - %llu \n", track_position);

	// Volume, track, setor and checksum, all in 4-and-4 format.
	if (bVerbose) printf(" Track %llu (c) / Sector %llu - Header Infos: %llu", track_number, sector_number, track_position);
	track_position = write_4_and_4(dest, track_position, sector_number);
	if (bVerbose) printf(" - %llu \n", track_position);

	// Epilogue. (removed)
	

	// Write gap 2.
	if (bVerbose) printf(" Track %llu (c) / Sector %llu - Gap2: %llu", track_number, sector_number, track_position);
	for (size_t c = 0; c < 7; ++c) {
		track_position = write_sync(dest, track_position);
	}
	if (bVerbose) printf(" - %llu \n", track_position);

	/*
		Write the sector body.
	*/

	// Prologue.
	if (bVerbose) printf(" Track %llu (c) / Sector %llu - Data Prologue: %llu", track_number, sector_number, track_position);
	track_position = write_byte(dest, track_position, 0xd5);
	track_position = write_byte(dest, track_position, 0xaa);
	track_position = write_byte(dest, track_position, 0xad);
	if (bVerbose) printf(" - %llu \n", track_position);

	// Sector contents.
	uint8_t contents[172];
	encode_6_and_2_128(contents, src);
	if (bVerbose) printf(" Track %llu (c) / Sector %llu - Data Content: %llu", track_number, sector_number, track_position);
	for (size_t c = 0; c < sizeof(contents); ++c) {
		track_position = write_byte(dest, track_position, contents[c]);
	}
	if (bVerbose) printf(" - %llu \n", track_position);

	// Epilogue. (removed)

	// Write gap 3.
	if (bVerbose) printf(" Track %llu (c) / Sector %llu - Gap3: %llu", track_number, sector_number, track_position);
	for (size_t c = 0; c < 8; ++c) {
		track_position = write_sync(dest, track_position);
	}
	if (bVerbose) printf(" - %llu \n", track_position);
}
