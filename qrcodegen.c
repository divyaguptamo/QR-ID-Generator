#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "qrcodegen.h"

#ifndef QRCODEGEN_TEST
	#define testable static  
#else
	#define testable  
#endif
testable void appendBitsToBuffer(unsigned int val, int numBits, uint8_t buffer[], int *bitLen);

testable void addEccAndInterleave(uint8_t data[], int version, enum qrcodegen_Ecc ecl, uint8_t result[]);
testable int getNumDataCodewords(int version, enum qrcodegen_Ecc ecl);
testable int getNumRawDataModules(int ver);

testable void reedSolomonComputeDivisor(int degree, uint8_t result[]);
testable void reedSolomonComputeRemainder(const uint8_t data[], int dataLen,
	const uint8_t generator[], int degree, uint8_t result[]);
testable uint8_t reedSolomonMultiply(uint8_t x, uint8_t y);

testable void initializeFunctionModules(int version, uint8_t qrcode[]);
static void drawLightFunctionModules(uint8_t qrcode[], int version);
static void drawFormatBits(enum qrcodegen_Ecc ecl, enum qrcodegen_Mask mask, uint8_t qrcode[]);
testable int getAlignmentPatternPositions(int version, uint8_t result[7]);
static void fillRectangle(int left, int top, int width, int height, uint8_t qrcode[]);

static void drawCodewords(const uint8_t data[], int dataLen, uint8_t qrcode[]);
static void applyMask(const uint8_t functionModules[], uint8_t qrcode[], enum qrcodegen_Mask mask);
static long getPenaltyScore(const uint8_t qrcode[]);
static int finderPenaltyCountPatterns(const int runHistory[7], int qrsize);
static int finderPenaltyTerminateAndCount(bool currentRunColor, int currentRunLength, int runHistory[7], int qrsize);
static void finderPenaltyAddHistory(int currentRunLength, int runHistory[7], int qrsize);

testable bool getModuleBounded(const uint8_t qrcode[], int x, int y);
testable void setModuleBounded(uint8_t qrcode[], int x, int y, bool isDark);
testable void setModuleUnbounded(uint8_t qrcode[], int x, int y, bool isDark);
static bool getBit(int x, int i);

testable int calcSegmentBitLength(enum qrcodegen_Mode mode, size_t numChars);
testable int getTotalBits(const struct qrcodegen_Segment segs[], size_t len, int version);
static int numCharCountBits(enum qrcodegen_Mode mode, int version);

static const char *ALPHANUMERIC_CHARSET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

#define LENGTH_OVERFLOW -1

testable const int8_t ECC_CODEWORDS_PER_BLOCK[4][41] = {
	
	{-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Low
	{-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},  // Medium
	{-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // Quartile
	{-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},  // High
};

#define qrcodegen_REED_SOLOMON_DEGREE_MAX 30  

testable const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
	
	{-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},  // Low
	{-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},  // Medium
	{-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},  // Quartile
	{-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},  // High
};

static const int PENALTY_N1 =  3;
static const int PENALTY_N2 =  3;
static const int PENALTY_N3 = 40;
static const int PENALTY_N4 = 10;

bool qrcodegen_encodeText(const char *text, uint8_t tempBuffer[], uint8_t qrcode[],
		enum qrcodegen_Ecc ecl, int minVersion, int maxVersion, enum qrcodegen_Mask mask, bool boostEcl) {
	
	size_t textLen = strlen(text);
	if (textLen == 0)
		return qrcodegen_encodeSegmentsAdvanced(NULL, 0, ecl, minVersion, maxVersion, mask, boostEcl, tempBuffer, qrcode);
	size_t bufLen = (size_t)qrcodegen_BUFFER_LEN_FOR_VERSION(maxVersion);
	
	struct qrcodegen_Segment seg;
	if (qrcodegen_isNumeric(text)) {
		if (qrcodegen_calcSegmentBufferSize(qrcodegen_Mode_NUMERIC, textLen) > bufLen)
			goto fail;
		seg = qrcodegen_makeNumeric(text, tempBuffer);
	} else if (qrcodegen_isAlphanumeric(text)) {
		if (qrcodegen_calcSegmentBufferSize(qrcodegen_Mode_ALPHANUMERIC, textLen) > bufLen)
			goto fail;
		seg = qrcodegen_makeAlphanumeric(text, tempBuffer);
	} else {
		if (textLen > bufLen)
			goto fail;
		for (size_t i = 0; i < textLen; i++)
			tempBuffer[i] = (uint8_t)text[i];
		seg.mode = qrcodegen_Mode_BYTE;
		seg.bitLength = calcSegmentBitLength(seg.mode, textLen);
		if (seg.bitLength == LENGTH_OVERFLOW)
			goto fail;
		seg.numChars = (int)textLen;
		seg.data = tempBuffer;
	}
	return qrcodegen_encodeSegmentsAdvanced(&seg, 1, ecl, minVersion, maxVersion, mask, boostEcl, tempBuffer, qrcode);
	
fail:
	qrcode[0] = 0;  // Set size to invalid value for safety
	return false;
}
bool qrcodegen_encodeBinary(uint8_t dataAndTemp[], size_t dataLen, uint8_t qrcode[],
		enum qrcodegen_Ecc ecl, int minVersion, int maxVersion, enum qrcodegen_Mask mask, bool boostEcl) {
	
	struct qrcodegen_Segment seg;
	seg.mode = qrcodegen_Mode_BYTE;
	seg.bitLength = calcSegmentBitLength(seg.mode, dataLen);
	if (seg.bitLength == LENGTH_OVERFLOW) {
		qrcode[0] = 0;  
		return false;
	}
	seg.numChars = (int)dataLen;
	seg.data = dataAndTemp;
	return qrcodegen_encodeSegmentsAdvanced(&seg, 1, ecl, minVersion, maxVersion, mask, boostEcl, dataAndTemp, qrcode);
}
testable void appendBitsToBuffer(unsigned int val, int numBits, uint8_t buffer[], int *bitLen) {
	assert(0 <= numBits && numBits <= 16 && (unsigned long)val >> numBits == 0);
	for (int i = numBits - 1; i >= 0; i--, (*bitLen)++)
		buffer[*bitLen >> 3] |= ((val >> i) & 1) << (7 - (*bitLen & 7));
}
bool qrcodegen_encodeSegments(const struct qrcodegen_Segment segs[], size_t len,
		enum qrcodegen_Ecc ecl, uint8_t tempBuffer[], uint8_t qrcode[]) {
	return qrcodegen_encodeSegmentsAdvanced(segs, len, ecl,
		qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true, tempBuffer, qrcode);
}
bool qrcodegen_encodeSegmentsAdvanced(const struct qrcodegen_Segment segs[], size_t len, enum qrcodegen_Ecc ecl,
		int minVersion, int maxVersion, enum qrcodegen_Mask mask, bool boostEcl, uint8_t tempBuffer[], uint8_t qrcode[]) {
	assert(segs != NULL || len == 0);
	assert(qrcodegen_VERSION_MIN <= minVersion && minVersion <= maxVersion && maxVersion <= qrcodegen_VERSION_MAX);
	assert(0 <= (int)ecl && (int)ecl <= 3 && -1 <= (int)mask && (int)mask <= 7);
	
	int version, dataUsedBits;
	for (version = minVersion; ; version++) {
		int dataCapacityBits = getNumDataCodewords(version, ecl) * 8;  // Number of data bits available
		dataUsedBits = getTotalBits(segs, len, version);
		if (dataUsedBits != LENGTH_OVERFLOW && dataUsedBits <= dataCapacityBits)
			break;  // This version number is found to be suitable
		if (version >= maxVersion) {  // All versions in the range could not fit the given data
			qrcode[0] = 0;  // Set size to invalid value for safety
			return false;
		}
	}
	assert(dataUsedBits != LENGTH_OVERFLOW);
	
	// Increase the error correction level while the data still fits in the current version number
	for (int i = (int)qrcodegen_Ecc_MEDIUM; i <= (int)qrcodegen_Ecc_HIGH; i++) {  // From low to high
		if (boostEcl && dataUsedBits <= getNumDataCodewords(version, (enum qrcodegen_Ecc)i) * 8)
			ecl = (enum qrcodegen_Ecc)i;
	}
	memset(qrcode, 0, (size_t)qrcodegen_BUFFER_LEN_FOR_VERSION(version) * sizeof(qrcode[0]));
	int bitLen = 0;
	for (size_t i = 0; i < len; i++) {
		const struct qrcodegen_Segment *seg = &segs[i];
		appendBitsToBuffer((unsigned int)seg->mode, 4, qrcode, &bitLen);
		appendBitsToBuffer((unsigned int)seg->numChars, numCharCountBits(seg->mode, version), qrcode, &bitLen);
		for (int j = 0; j < seg->bitLength; j++) {
			int bit = (seg->data[j >> 3] >> (7 - (j & 7))) & 1;
			appendBitsToBuffer((unsigned int)bit, 1, qrcode, &bitLen);
		}
	}
	assert(bitLen == dataUsedBits);
	int dataCapacityBits = getNumDataCodewords(version, ecl) * 8;
	assert(bitLen <= dataCapacityBits);
	int terminatorBits = dataCapacityBits - bitLen;
	if (terminatorBits > 4)
		terminatorBits = 4;
	appendBitsToBuffer(0, terminatorBits, qrcode, &bitLen);
	appendBitsToBuffer(0, (8 - bitLen % 8) % 8, qrcode, &bitLen);
	assert(bitLen % 8 == 0);
	
	for (uint8_t padByte = 0xEC; bitLen < dataCapacityBits; padByte ^= 0xEC ^ 0x11)
		appendBitsToBuffer(padByte, 8, qrcode, &bitLen);
	addEccAndInterleave(qrcode, version, ecl, tempBuffer);
	initializeFunctionModules(version, qrcode);
	drawCodewords(tempBuffer, getNumRawDataModules(version) / 8, qrcode);
	drawLightFunctionModules(qrcode, version);
	initializeFunctionModules(version, tempBuffer);
	
	if (mask == qrcodegen_Mask_AUTO) {  // Automatically choose best mask
		long minPenalty = LONG_MAX;
		for (int i = 0; i < 8; i++) {
			enum qrcodegen_Mask msk = (enum qrcodegen_Mask)i;
			applyMask(tempBuffer, qrcode, msk);
			drawFormatBits(ecl, msk, qrcode);
			long penalty = getPenaltyScore(qrcode);
			if (penalty < minPenalty) {
				mask = msk;
				minPenalty = penalty;
			}
			applyMask(tempBuffer, qrcode, msk);  // Undoes the mask due to XOR
		}
	}
	assert(0 <= (int)mask && (int)mask <= 7);
	applyMask(tempBuffer, qrcode, mask);  // Apply the final choice of mask
	drawFormatBits(ecl, mask, qrcode);  // Overwrite old format bits
	return true;
}
testable void addEccAndInterleave(uint8_t data[], int version, enum qrcodegen_Ecc ecl, uint8_t result[]) {
	// Calculate parameter numbers
	assert(0 <= (int)ecl && (int)ecl < 4 && qrcodegen_VERSION_MIN <= version && version <= qrcodegen_VERSION_MAX);
	int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[(int)ecl][version];
	int blockEccLen = ECC_CODEWORDS_PER_BLOCK  [(int)ecl][version];
	int rawCodewords = getNumRawDataModules(version) / 8;
	int dataLen = getNumDataCodewords(version, ecl);
	int numShortBlocks = numBlocks - rawCodewords % numBlocks;
	int shortBlockDataLen = rawCodewords / numBlocks - blockEccLen;
	uint8_t rsdiv[qrcodegen_REED_SOLOMON_DEGREE_MAX];
	reedSolomonComputeDivisor(blockEccLen, rsdiv);
	const uint8_t *dat = data;
	for (int i = 0; i < numBlocks; i++) {
		int datLen = shortBlockDataLen + (i < numShortBlocks ? 0 : 1);
		uint8_t *ecc = &data[dataLen];  // Temporary storage
		reedSolomonComputeRemainder(dat, datLen, rsdiv, blockEccLen, ecc);
		for (int j = 0, k = i; j < datLen; j++, k += numBlocks) {  // Copy data
			if (j == shortBlockDataLen)
				k -= numShortBlocks;
			result[k] = dat[j];
		}
		for (int j = 0, k = dataLen + i; j < blockEccLen; j++, k += numBlocks)  // Copy ECC
			result[k] = ecc[j];
		dat += datLen;
	}
}

testable int getNumDataCodewords(int version, enum qrcodegen_Ecc ecl) {
	int v = version, e = (int)ecl;
	assert(0 <= e && e < 4);
	return getNumRawDataModules(v) / 8
		- ECC_CODEWORDS_PER_BLOCK    [e][v]
		* NUM_ERROR_CORRECTION_BLOCKS[e][v];
}
testable int getNumRawDataModules(int ver) {
	assert(qrcodegen_VERSION_MIN <= ver && ver <= qrcodegen_VERSION_MAX);
	int result = (16 * ver + 128) * ver + 64;
	if (ver >= 2) {
		int numAlign = ver / 7 + 2;
		result -= (25 * numAlign - 10) * numAlign - 55;
		if (ver >= 7)
			result -= 36;
	}
	assert(208 <= result && result <= 29648);
	return result;
}
testable void reedSolomonComputeDivisor(int degree, uint8_t result[]) {
	assert(1 <= degree && degree <= qrcodegen_REED_SOLOMON_DEGREE_MAX);
	memset(result, 0, (size_t)degree * sizeof(result[0]));
	result[degree - 1] = 1;  // Start off with the monomial x^0
	uint8_t root = 1;
	for (int i = 0; i < degree; i++) {
		// Multiply the current product by (x - r^i)
		for (int j = 0; j < degree; j++) {
			result[j] = reedSolomonMultiply(result[j], root);
			if (j + 1 < degree)
				result[j] ^= result[j + 1];
		}
		root = reedSolomonMultiply(root, 0x02);
	}
}
testable void reedSolomonComputeRemainder(const uint8_t data[], int dataLen,
		const uint8_t generator[], int degree, uint8_t result[]) {
	assert(1 <= degree && degree <= qrcodegen_REED_SOLOMON_DEGREE_MAX);
	memset(result, 0, (size_t)degree * sizeof(result[0]));
	for (int i = 0; i < dataLen; i++) {  // Polynomial division
		uint8_t factor = data[i] ^ result[0];
		memmove(&result[0], &result[1], (size_t)(degree - 1) * sizeof(result[0]));
		result[degree - 1] = 0;
		for (int j = 0; j < degree; j++)
			result[j] ^= reedSolomonMultiply(generator[j], factor);
	}
}

#undef qrcodegen_REED_SOLOMON_DEGREE_MAX
testable uint8_t reedSolomonMultiply(uint8_t x, uint8_t y) {
	// Russian peasant multiplication
	uint8_t z = 0;
	for (int i = 7; i >= 0; i--) {
		z = (uint8_t)((z << 1) ^ ((z >> 7) * 0x11D));
		z ^= ((y >> i) & 1) * x;
	}
	return z;
}

testable void initializeFunctionModules(int version, uint8_t qrcode[]) {
	// Initialize QR Code
	int qrsize = version * 4 + 17;
	memset(qrcode, 0, (size_t)((qrsize * qrsize + 7) / 8 + 1) * sizeof(qrcode[0]));
	qrcode[0] = (uint8_t)qrsize;
	
	// Fill horizontal and vertical timing patterns
	fillRectangle(6, 0, 1, qrsize, qrcode);
	fillRectangle(0, 6, qrsize, 1, qrcode);
	
	// Fill 3 finder patterns (all corners except bottom right) and format bits
	fillRectangle(0, 0, 9, 9, qrcode);
	fillRectangle(qrsize - 8, 0, 8, 9, qrcode);
	fillRectangle(0, qrsize - 8, 9, 8, qrcode);
	
	// Fill numerous alignment patterns
	uint8_t alignPatPos[7];
	int numAlign = getAlignmentPatternPositions(version, alignPatPos);
	for (int i = 0; i < numAlign; i++) {
		for (int j = 0; j < numAlign; j++) {
			// Don't draw on the three finder corners
			if (!((i == 0 && j == 0) || (i == 0 && j == numAlign - 1) || (i == numAlign - 1 && j == 0)))
				fillRectangle(alignPatPos[i] - 2, alignPatPos[j] - 2, 5, 5, qrcode);
		}
	}
	
	// Fill version blocks
	if (version >= 7) {
		fillRectangle(qrsize - 11, 0, 3, 6, qrcode);
		fillRectangle(0, qrsize - 11, 6, 3, qrcode);
	}
}
static void drawLightFunctionModules(uint8_t qrcode[], int version) {
	// Draw horizontal and vertical timing patterns
	int qrsize = qrcodegen_getSize(qrcode);
	for (int i = 7; i < qrsize - 7; i += 2) {
		setModuleBounded(qrcode, 6, i, false);
		setModuleBounded(qrcode, i, 6, false);
	}
	
	// Draw 3 finder patterns (all corners except bottom right; overwrites some timing modules)
	for (int dy = -4; dy <= 4; dy++) {
		for (int dx = -4; dx <= 4; dx++) {
			int dist = abs(dx);
			if (abs(dy) > dist)
				dist = abs(dy);
			if (dist == 2 || dist == 4) {
				setModuleUnbounded(qrcode, 3 + dx, 3 + dy, false);
				setModuleUnbounded(qrcode, qrsize - 4 + dx, 3 + dy, false);
				setModuleUnbounded(qrcode, 3 + dx, qrsize - 4 + dy, false);
			}
		}
	}
	
	// Draw numerous alignment patterns
	uint8_t alignPatPos[7];
	int numAlign = getAlignmentPatternPositions(version, alignPatPos);
	for (int i = 0; i < numAlign; i++) {
		for (int j = 0; j < numAlign; j++) {
			if ((i == 0 && j == 0) || (i == 0 && j == numAlign - 1) || (i == numAlign - 1 && j == 0))
				continue;  // Don't draw on the three finder corners
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++)
					setModuleBounded(qrcode, alignPatPos[i] + dx, alignPatPos[j] + dy, dx == 0 && dy == 0);
			}
		}
	}
	
	// Draw version blocks
	if (version >= 7) {
		// Calculate error correction code and pack bits
		int rem = version;  // version is uint6, in the range [7, 40]
		for (int i = 0; i < 12; i++)
			rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
		long bits = (long)version << 12 | rem;  // uint18
		assert(bits >> 18 == 0);
		
		// Draw two copies
		for (int i = 0; i < 6; i++) {
			for (int j = 0; j < 3; j++) {
				int k = qrsize - 11 + j;
				setModuleBounded(qrcode, k, i, (bits & 1) != 0);
				setModuleBounded(qrcode, i, k, (bits & 1) != 0);
				bits >>= 1;
			}
		}
	}
}
static void drawFormatBits(enum qrcodegen_Ecc ecl, enum qrcodegen_Mask mask, uint8_t qrcode[]) {
	// Calculate error correction code and pack bits
	assert(0 <= (int)mask && (int)mask <= 7);
	static const int table[] = {1, 0, 3, 2};
	int data = table[(int)ecl] << 3 | (int)mask;  // errCorrLvl is uint2, mask is uint3
	int rem = data;
	for (int i = 0; i < 10; i++)
		rem = (rem << 1) ^ ((rem >> 9) * 0x537);
	int bits = (data << 10 | rem) ^ 0x5412;  // uint15
	assert(bits >> 15 == 0);
	
	// Draw first copy
	for (int i = 0; i <= 5; i++)
		setModuleBounded(qrcode, 8, i, getBit(bits, i));
	setModuleBounded(qrcode, 8, 7, getBit(bits, 6));
	setModuleBounded(qrcode, 8, 8, getBit(bits, 7));
	setModuleBounded(qrcode, 7, 8, getBit(bits, 8));
	for (int i = 9; i < 15; i++)
		setModuleBounded(qrcode, 14 - i, 8, getBit(bits, i));
	
	// Draw second copy
	int qrsize = qrcodegen_getSize(qrcode);
	for (int i = 0; i < 8; i++)
		setModuleBounded(qrcode, qrsize - 1 - i, 8, getBit(bits, i));
	for (int i = 8; i < 15; i++)
		setModuleBounded(qrcode, 8, qrsize - 15 + i, getBit(bits, i));
	setModuleBounded(qrcode, 8, qrsize - 8, true);  // Always dark
}
testable int getAlignmentPatternPositions(int version, uint8_t result[7]) {
	if (version == 1)
		return 0;
	int numAlign = version / 7 + 2;
	int step = (version * 8 + numAlign * 3 + 5) / (numAlign * 4 - 4) * 2;
	for (int i = numAlign - 1, pos = version * 4 + 10; i >= 1; i--, pos -= step)
		result[i] = (uint8_t)pos;
	result[0] = 6;
	return numAlign;
}


// Sets every module in the range [left : left + width] * [top : top + height] to dark.
static void fillRectangle(int left, int top, int width, int height, uint8_t qrcode[]) {
	for (int dy = 0; dy < height; dy++) {
		for (int dx = 0; dx < width; dx++)
			setModuleBounded(qrcode, left + dx, top + dy, true);
	}
}

static void drawCodewords(const uint8_t data[], int dataLen, uint8_t qrcode[]) {
	int qrsize = qrcodegen_getSize(qrcode);
	int i = 0;  // Bit index into the data
	// Do the funny zigzag scan
	for (int right = qrsize - 1; right >= 1; right -= 2) {  // Index of right column in each column pair
		if (right == 6)
			right = 5;
		for (int vert = 0; vert < qrsize; vert++) {  // Vertical counter
			for (int j = 0; j < 2; j++) {
				int x = right - j;  // Actual x coordinate
				bool upward = ((right + 1) & 2) == 0;
				int y = upward ? qrsize - 1 - vert : vert;  // Actual y coordinate
				if (!getModuleBounded(qrcode, x, y) && i < dataLen * 8) {
					bool dark = getBit(data[i >> 3], 7 - (i & 7));
					setModuleBounded(qrcode, x, y, dark);
					i++;
				}
				// If this QR Code has any remainder bits (0 to 7), they were assigned as
				// 0/false/light by the constructor and are left unchanged by this method
			}
		}
	}
	assert(i == dataLen * 8);
}

static void applyMask(const uint8_t functionModules[], uint8_t qrcode[], enum qrcodegen_Mask mask) {
	assert(0 <= (int)mask && (int)mask <= 7);  // Disallows qrcodegen_Mask_AUTO
	int qrsize = qrcodegen_getSize(qrcode);
	for (int y = 0; y < qrsize; y++) {
		for (int x = 0; x < qrsize; x++) {
			if (getModuleBounded(functionModules, x, y))
				continue;
			bool invert;
			switch ((int)mask) {
				case 0:  invert = (x + y) % 2 == 0;                    break;
				case 1:  invert = y % 2 == 0;                          break;
				case 2:  invert = x % 3 == 0;                          break;
				case 3:  invert = (x + y) % 3 == 0;                    break;
				case 4:  invert = (x / 3 + y / 2) % 2 == 0;            break;
				case 5:  invert = x * y % 2 + x * y % 3 == 0;          break;
				case 6:  invert = (x * y % 2 + x * y % 3) % 2 == 0;    break;
				case 7:  invert = ((x + y) % 2 + x * y % 3) % 2 == 0;  break;
				default:  assert(false);  return;
			}
			bool val = getModuleBounded(qrcode, x, y);
			setModuleBounded(qrcode, x, y, val ^ invert);
		}
	}
}
static long getPenaltyScore(const uint8_t qrcode[]) {
	int qrsize = qrcodegen_getSize(qrcode);
	long result = 0;
	
	// Adjacent modules in row having same color, and finder-like patterns
	for (int y = 0; y < qrsize; y++) {
		bool runColor = false;
		int runX = 0;
		int runHistory[7] = {0};
		for (int x = 0; x < qrsize; x++) {
			if (getModuleBounded(qrcode, x, y) == runColor) {
				runX++;
				if (runX == 5)
					result += PENALTY_N1;
				else if (runX > 5)
					result++;
			} else {
				finderPenaltyAddHistory(runX, runHistory, qrsize);
				if (!runColor)
					result += finderPenaltyCountPatterns(runHistory, qrsize) * PENALTY_N3;
				runColor = getModuleBounded(qrcode, x, y);
				runX = 1;
			}
		}
		result += finderPenaltyTerminateAndCount(runColor, runX, runHistory, qrsize) * PENALTY_N3;
	}
	// Adjacent modules in column having same color, and finder-like patterns
	for (int x = 0; x < qrsize; x++) {
		bool runColor = false;
		int runY = 0;
		int runHistory[7] = {0};
		for (int y = 0; y < qrsize; y++) {
			if (getModuleBounded(qrcode, x, y) == runColor) {
				runY++;
				if (runY == 5)
					result += PENALTY_N1;
				else if (runY > 5)
					result++;
			} else {
				finderPenaltyAddHistory(runY, runHistory, qrsize);
				if (!runColor)
					result += finderPenaltyCountPatterns(runHistory, qrsize) * PENALTY_N3;
				runColor = getModuleBounded(qrcode, x, y);
				runY = 1;
			}
		}
		result += finderPenaltyTerminateAndCount(runColor, runY, runHistory, qrsize) * PENALTY_N3;
	}
	
	// 2*2 blocks of modules having same color
	for (int y = 0; y < qrsize - 1; y++) {
		for (int x = 0; x < qrsize - 1; x++) {
			bool  color = getModuleBounded(qrcode, x, y);
			if (  color == getModuleBounded(qrcode, x + 1, y) &&
			      color == getModuleBounded(qrcode, x, y + 1) &&
			      color == getModuleBounded(qrcode, x + 1, y + 1))
				result += PENALTY_N2;
		}
	}
	
	// Balance of dark and light modules
	int dark = 0;
	for (int y = 0; y < qrsize; y++) {
		for (int x = 0; x < qrsize; x++) {
			if (getModuleBounded(qrcode, x, y))
				dark++;
		}
	}
	int total = qrsize * qrsize;  // Note that size is odd, so dark/total != 1/2
	// Compute the smallest integer k >= 0 such that (45-5k)% <= dark/total <= (55+5k)%
	int k = (int)((labs(dark * 20L - total * 10L) + total - 1) / total) - 1;
	assert(0 <= k && k <= 9);
	result += k * PENALTY_N4;
	assert(0 <= result && result <= 2568888L);  // Non-tight upper bound based on default values of PENALTY_N1, ..., N4
	return result;
}


// Can only be called immediately after a light run is added, and
// returns either 0, 1, or 2. A helper function for getPenaltyScore().
static int finderPenaltyCountPatterns(const int runHistory[7], int qrsize) {
	int n = runHistory[1];
	assert(n <= qrsize * 3);  (void)qrsize;
	bool core = n > 0 && runHistory[2] == n && runHistory[3] == n * 3 && runHistory[4] == n && runHistory[5] == n;
	// The maximum QR Code size is 177, hence the dark run length n <= 177.
	// Arithmetic is promoted to int, so n*4 will not overflow.
	return (core && runHistory[0] >= n * 4 && runHistory[6] >= n ? 1 : 0)
	     + (core && runHistory[6] >= n * 4 && runHistory[0] >= n ? 1 : 0);
}


// Must be called at the end of a line (row or column) of modules. A helper function for getPenaltyScore().
static int finderPenaltyTerminateAndCount(bool currentRunColor, int currentRunLength, int runHistory[7], int qrsize) {
	if (currentRunColor) {  // Terminate dark run
		finderPenaltyAddHistory(currentRunLength, runHistory, qrsize);
		currentRunLength = 0;
	}
	currentRunLength += qrsize;  // Add light border to final run
	finderPenaltyAddHistory(currentRunLength, runHistory, qrsize);
	return finderPenaltyCountPatterns(runHistory, qrsize);
}


// Pushes the given value to the front and drops the last value. A helper function for getPenaltyScore().
static void finderPenaltyAddHistory(int currentRunLength, int runHistory[7], int qrsize) {
	if (runHistory[0] == 0)
		currentRunLength += qrsize;  // Add light border to initial run
	memmove(&runHistory[1], &runHistory[0], 6 * sizeof(runHistory[0]));
	runHistory[0] = currentRunLength;
}



/*---- Basic QR Code information ----*/

// Public function - see documentation comment in header file.
int qrcodegen_getSize(const uint8_t qrcode[]) {
	assert(qrcode != NULL);
	int result = qrcode[0];
	assert((qrcodegen_VERSION_MIN * 4 + 17) <= result
		&& result <= (qrcodegen_VERSION_MAX * 4 + 17));
	return result;
}


// Public function - see documentation comment in header file.
bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y) {
	assert(qrcode != NULL);
	int qrsize = qrcode[0];
	return (0 <= x && x < qrsize && 0 <= y && y < qrsize) && getModuleBounded(qrcode, x, y);
}


// Returns the color of the module at the given coordinates, which must be in bounds.
testable bool getModuleBounded(const uint8_t qrcode[], int x, int y) {
	int qrsize = qrcode[0];
	assert(21 <= qrsize && qrsize <= 177 && 0 <= x && x < qrsize && 0 <= y && y < qrsize);
	int index = y * qrsize + x;
	return getBit(qrcode[(index >> 3) + 1], index & 7);
}


// Sets the color of the module at the given coordinates, which must be in bounds.
testable void setModuleBounded(uint8_t qrcode[], int x, int y, bool isDark) {
	int qrsize = qrcode[0];
	assert(21 <= qrsize && qrsize <= 177 && 0 <= x && x < qrsize && 0 <= y && y < qrsize);
	int index = y * qrsize + x;
	int bitIndex = index & 7;
	int byteIndex = (index >> 3) + 1;
	if (isDark)
		qrcode[byteIndex] |= 1 << bitIndex;
	else
		qrcode[byteIndex] &= (1 << bitIndex) ^ 0xFF;
}


// Sets the color of the module at the given coordinates, doing nothing if out of bounds.
testable void setModuleUnbounded(uint8_t qrcode[], int x, int y, bool isDark) {
	int qrsize = qrcode[0];
	if (0 <= x && x < qrsize && 0 <= y && y < qrsize)
		setModuleBounded(qrcode, x, y, isDark);
}


// Returns true iff the i'th bit of x is set to 1. Requires x >= 0 and 0 <= i <= 14.
static bool getBit(int x, int i) {
	return ((x >> i) & 1) != 0;
}



/*---- Segment handling ----*/

// Public function - see documentation comment in header file.
bool qrcodegen_isNumeric(const char *text) {
	assert(text != NULL);
	for (; *text != '\0'; text++) {
		if (*text < '0' || *text > '9')
			return false;
	}
	return true;
}


// Public function - see documentation comment in header file.
bool qrcodegen_isAlphanumeric(const char *text) {
	assert(text != NULL);
	for (; *text != '\0'; text++) {
		if (strchr(ALPHANUMERIC_CHARSET, *text) == NULL)
			return false;
	}
	return true;
}


// Public function - see documentation comment in header file.
size_t qrcodegen_calcSegmentBufferSize(enum qrcodegen_Mode mode, size_t numChars) {
	int temp = calcSegmentBitLength(mode, numChars);
	if (temp == LENGTH_OVERFLOW)
		return SIZE_MAX;
	assert(0 <= temp && temp <= INT16_MAX);
	return ((size_t)temp + 7) / 8;
}


// Returns the number of data bits needed to represent a segment
// containing the given number of characters using the given mode. Notes:
// - Returns LENGTH_OVERFLOW on failure, i.e. numChars > INT16_MAX
//   or the number of needed bits exceeds INT16_MAX (i.e. 32767).
// - Otherwise, all valid results are in the range [0, INT16_MAX].
// - For byte mode, numChars measures the number of bytes, not Unicode code points.
// - For ECI mode, numChars must be 0, and the worst-case number of bits is returned.
//   An actual ECI segment can have shorter data. For non-ECI modes, the result is exact.
testable int calcSegmentBitLength(enum qrcodegen_Mode mode, size_t numChars) {
	// All calculations are designed to avoid overflow on all platforms
	if (numChars > (unsigned int)INT16_MAX)
		return LENGTH_OVERFLOW;
	long result = (long)numChars;
	if (mode == qrcodegen_Mode_NUMERIC)
		result = (result * 10 + 2) / 3;  // ceil(10/3 * n)
	else if (mode == qrcodegen_Mode_ALPHANUMERIC)
		result = (result * 11 + 1) / 2;  // ceil(11/2 * n)
	else if (mode == qrcodegen_Mode_BYTE)
		result *= 8;
	else if (mode == qrcodegen_Mode_KANJI)
		result *= 13;
	else if (mode == qrcodegen_Mode_ECI && numChars == 0)
		result = 3 * 8;
	else {  // Invalid argument
		assert(false);
		return LENGTH_OVERFLOW;
	}
	assert(result >= 0);
	if (result > INT16_MAX)
		return LENGTH_OVERFLOW;
	return (int)result;
}


// Public function - see documentation comment in header file.
struct qrcodegen_Segment qrcodegen_makeBytes(const uint8_t data[], size_t len, uint8_t buf[]) {
	assert(data != NULL || len == 0);
	struct qrcodegen_Segment result;
	result.mode = qrcodegen_Mode_BYTE;
	result.bitLength = calcSegmentBitLength(result.mode, len);
	assert(result.bitLength != LENGTH_OVERFLOW);
	result.numChars = (int)len;
	if (len > 0)
		memcpy(buf, data, len * sizeof(buf[0]));
	result.data = buf;
	return result;
}


// Public function - see documentation comment in header file.
struct qrcodegen_Segment qrcodegen_makeNumeric(const char *digits, uint8_t buf[]) {
	assert(digits != NULL);
	struct qrcodegen_Segment result;
	size_t len = strlen(digits);
	result.mode = qrcodegen_Mode_NUMERIC;
	int bitLen = calcSegmentBitLength(result.mode, len);
	assert(bitLen != LENGTH_OVERFLOW);
	result.numChars = (int)len;
	if (bitLen > 0)
		memset(buf, 0, ((size_t)bitLen + 7) / 8 * sizeof(buf[0]));
	result.bitLength = 0;
	
	unsigned int accumData = 0;
	int accumCount = 0;
	for (; *digits != '\0'; digits++) {
		char c = *digits;
		assert('0' <= c && c <= '9');
		accumData = accumData * 10 + (unsigned int)(c - '0');
		accumCount++;
		if (accumCount == 3) {
			appendBitsToBuffer(accumData, 10, buf, &result.bitLength);
			accumData = 0;
			accumCount = 0;
		}
	}
	if (accumCount > 0)  // 1 or 2 digits remaining
		appendBitsToBuffer(accumData, accumCount * 3 + 1, buf, &result.bitLength);
	assert(result.bitLength == bitLen);
	result.data = buf;
	return result;
}


// Public function - see documentation comment in header file.
struct qrcodegen_Segment qrcodegen_makeAlphanumeric(const char *text, uint8_t buf[]) {
	assert(text != NULL);
	struct qrcodegen_Segment result;
	size_t len = strlen(text);
	result.mode = qrcodegen_Mode_ALPHANUMERIC;
	int bitLen = calcSegmentBitLength(result.mode, len);
	assert(bitLen != LENGTH_OVERFLOW);
	result.numChars = (int)len;
	if (bitLen > 0)
		memset(buf, 0, ((size_t)bitLen + 7) / 8 * sizeof(buf[0]));
	result.bitLength = 0;
	
	unsigned int accumData = 0;
	int accumCount = 0;
	for (; *text != '\0'; text++) {
		const char *temp = strchr(ALPHANUMERIC_CHARSET, *text);
		assert(temp != NULL);
		accumData = accumData * 45 + (unsigned int)(temp - ALPHANUMERIC_CHARSET);
		accumCount++;
		if (accumCount == 2) {
			appendBitsToBuffer(accumData, 11, buf, &result.bitLength);
			accumData = 0;
			accumCount = 0;
		}
	}
	if (accumCount > 0)  // 1 character remaining
		appendBitsToBuffer(accumData, 6, buf, &result.bitLength);
	assert(result.bitLength == bitLen);
	result.data = buf;
	return result;
}


// Public function - see documentation comment in header file.
struct qrcodegen_Segment qrcodegen_makeEci(long assignVal, uint8_t buf[]) {
	struct qrcodegen_Segment result;
	result.mode = qrcodegen_Mode_ECI;
	result.numChars = 0;
	result.bitLength = 0;
	if (assignVal < 0)
		assert(false);
	else if (assignVal < (1 << 7)) {
		memset(buf, 0, 1 * sizeof(buf[0]));
		appendBitsToBuffer((unsigned int)assignVal, 8, buf, &result.bitLength);
	} else if (assignVal < (1 << 14)) {
		memset(buf, 0, 2 * sizeof(buf[0]));
		appendBitsToBuffer(2, 2, buf, &result.bitLength);
		appendBitsToBuffer((unsigned int)assignVal, 14, buf, &result.bitLength);
	} else if (assignVal < 1000000L) {
		memset(buf, 0, 3 * sizeof(buf[0]));
		appendBitsToBuffer(6, 3, buf, &result.bitLength);
		appendBitsToBuffer((unsigned int)(assignVal >> 10), 11, buf, &result.bitLength);
		appendBitsToBuffer((unsigned int)(assignVal & 0x3FF), 10, buf, &result.bitLength);
	} else
		assert(false);
	result.data = buf;
	return result;
}
testable int getTotalBits(const struct qrcodegen_Segment segs[], size_t len, int version) {
	assert(segs != NULL || len == 0);
	long result = 0;
	for (size_t i = 0; i < len; i++) {
		int numChars  = segs[i].numChars;
		int bitLength = segs[i].bitLength;
		assert(0 <= numChars  && numChars  <= INT16_MAX);
		assert(0 <= bitLength && bitLength <= INT16_MAX);
		int ccbits = numCharCountBits(segs[i].mode, version);
		assert(0 <= ccbits && ccbits <= 16);
		if (numChars >= (1L << ccbits))
			return LENGTH_OVERFLOW;  // The segment's length doesn't fit the field's bit width
		result += 4L + ccbits + bitLength;
		if (result > INT16_MAX)
			return LENGTH_OVERFLOW;  // The sum might overflow an int type
	}
	assert(0 <= result && result <= INT16_MAX);
	return (int)result;
}
static int numCharCountBits(enum qrcodegen_Mode mode, int version) {
	assert(qrcodegen_VERSION_MIN <= version && version <= qrcodegen_VERSION_MAX);
	int i = (version + 7) / 17;
	switch (mode) {
		case qrcodegen_Mode_NUMERIC     : { static const int temp[] = {10, 12, 14}; return temp[i]; }
		case qrcodegen_Mode_ALPHANUMERIC: { static const int temp[] = { 9, 11, 13}; return temp[i]; }
		case qrcodegen_Mode_BYTE        : { static const int temp[] = { 8, 16, 16}; return temp[i]; }
		case qrcodegen_Mode_KANJI       : { static const int temp[] = { 8, 10, 12}; return temp[i]; }
		case qrcodegen_Mode_ECI         : return 0;
		default:  assert(false);  return -1;  // Dummy value
	}
}


#undef LENGTH_OVERFLOW
