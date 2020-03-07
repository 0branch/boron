#define boot_len	1041
static const unsigned char boot_data[] = {
  0x42,0x4F,0x52,0x32,0x0,0x0,0x2,0x8D,
  0x0,0x0,0x0,0x26,0x17,0x30,0xF,0x0,
  0x0,0xD,0x0,0x1,0xD,0x0,0x2,0x17,
  0x1,0x0,0x8F,0x0,0x3,0x10,0x0,0x4,
  0xF,0x0,0x5,0xD,0x0,0x6,0xF,0x0,
  0x7,0xD,0x0,0x8,0x8F,0x0,0x9,0x10,
  0x0,0xA,0x8F,0x0,0xB,0x10,0x0,0xC,
  0x8F,0x0,0xD,0x10,0x0,0xE,0x8F,0x0,
  0xF,0xD,0x0,0x10,0x17,0x2,0x0,0x17,
  0x3,0x0,0x8F,0x0,0x11,0xD,0x0,0x10,
  0x17,0x4,0x0,0x17,0x5,0x0,0x8F,0x0,
  0x12,0xD,0x0,0x10,0x17,0x6,0x0,0x17,
  0x7,0x0,0x8F,0x0,0x13,0xD,0x0,0x10,
  0x17,0x8,0x0,0x17,0x9,0x0,0x8F,0x0,
  0x14,0xD,0x0,0x10,0x17,0xA,0x0,0x17,
  0xB,0x0,0x8F,0x0,0x15,0xD,0x0,0x10,
  0x17,0xC,0x0,0x17,0xD,0x0,0x8F,0x0,
  0x16,0xD,0x0,0x10,0x17,0xE,0x0,0x17,
  0xF,0x0,0x8F,0x0,0x17,0xD,0x0,0x10,
  0x17,0x10,0x0,0x17,0x11,0x0,0x17,0x6,
  0x8F,0x0,0x18,0xA,0x3,0x4,0x0,0x4,
  0x8F,0x0,0x19,0xF,0x0,0x1A,0xF,0x0,
  0x1B,0xD,0x0,0x1C,0x17,0x2,0xD,0x0,
  0x1D,0xD,0x0,0x1E,0x17,0x3,0xD,0x0,
  0x1,0xD,0x0,0x2,0xD,0x0,0x1D,0x17,
  0x2,0xD,0x0,0x1F,0x1,0x63,0x0,0x10,
  0x0,0x10,0x0,0x17,0x3,0xD,0x0,0x1,
  0xD,0x0,0x20,0xD,0x0,0x1F,0x17,0x3,
  0xD,0x0,0x1F,0xD,0x0,0x21,0x11,0x0,
  0x22,0x17,0x4,0xD,0x0,0x23,0xD,0x0,
  0x1,0xD,0x0,0x24,0xD,0x0,0x1F,0x17,
  0x2,0xD,0x0,0x25,0xD,0x0,0x1D,0x17,
  0xA,0x8F,0x0,0x25,0xD,0x0,0x26,0xD,
  0x0,0x27,0xD,0x0,0x25,0x17,0x12,0x0,
  0x17,0x13,0x0,0x8D,0x0,0x28,0xD,0x0,
  0x25,0xD,0x0,0x29,0xD,0x0,0x1D,0x17,
  0x2,0xD,0x0,0x1D,0xD,0x0,0x1E,0x17,
  0xF,0x8D,0x0,0x2A,0xD,0x0,0xC,0xF,
  0x0,0x1D,0xD,0x0,0x29,0xD,0x0,0x1D,
  0x17,0x14,0x0,0x8D,0x0,0x28,0xD,0x0,
  0x26,0xD,0x0,0x27,0xD,0x0,0x2B,0xD,
  0x0,0x1D,0x97,0x15,0x0,0x97,0x16,0x0,
  0x8D,0x0,0x2C,0xD,0x0,0x1D,0x17,0x4,
  0xD,0x0,0x2D,0xD,0x0,0x2E,0xD,0x0,
  0x2F,0x11,0x0,0x30,0x17,0xB,0x8F,0x0,
  0x31,0xD,0x0,0x26,0xD,0x0,0x27,0xD,
  0x0,0x2E,0x17,0x17,0x0,0x17,0x18,0x0,
  0x8D,0x0,0x26,0xD,0x0,0x30,0x17,0x19,
  0x0,0x17,0x1A,0x0,0x8D,0x0,0x2D,0x17,
  0x1,0xD,0x0,0x32,0x17,0x7,0x8D,0x0,
  0x26,0xF,0x0,0x33,0x19,0x1B,0x0,0xD,
  0x0,0x32,0x4,0x2F,0x97,0x1C,0x0,0x97,
  0x1D,0x0,0x17,0x1,0xD,0x0,0x32,0x17,
  0x3,0x19,0x1E,0x0,0xD,0x0,0x32,0x4,
  0x2F,0x17,0x2,0xD,0x0,0x34,0xD,0x0,
  0x25,0x17,0x2,0xD,0x0,0x35,0xD,0x0,
  0x25,0x17,0x2,0xD,0x0,0x36,0xD,0x0,
  0x1D,0x17,0x3,0xD,0x0,0x34,0xD,0x0,
  0x2B,0xD,0x0,0x1D,0x17,0x3,0xD,0x0,
  0x35,0xD,0x0,0x2B,0xD,0x0,0x1D,0x17,
  0x2,0xD,0x0,0x37,0xD,0x0,0x2E,0x17,
  0x1,0x5,0x2,0x17,0x5,0x8F,0x0,0x38,
  0xD,0x0,0x2D,0x8D,0x0,0x39,0x17,0x1F,
  0x0,0x17,0x20,0x0,0x17,0x6,0x8D,0x0,
  0x2A,0xF,0x0,0x38,0xD,0x0,0x3A,0xD,
  0x0,0x2D,0xD,0x0,0x2E,0x17,0x21,0x0,
  0x17,0x2,0xD,0x0,0x3A,0xD,0x0,0x3B,
  0x17,0x4,0xD,0x0,0x3C,0xD,0x0,0x33,
  0xD,0x0,0x29,0x17,0x22,0x0,0x17,0x2,
  0xD,0x0,0x29,0x17,0x23,0x0,0x17,0x2,
  0xD,0x0,0x3D,0xD,0x0,0x3E,0x17,0x4,
  0xF,0x0,0x38,0xD,0x0,0x3A,0xD,0x0,
  0x38,0xD,0x0,0x2E,0x17,0x5,0xF,0x0,
  0x38,0x19,0x24,0x0,0xD,0x0,0x38,0xD,
  0x0,0x2F,0xD,0x0,0x31,0x17,0x4,0x19,
  0x25,0x0,0xD,0x0,0x38,0xD,0x0,0x2F,
  0xD,0x0,0x31,0x17,0x4,0xD,0x0,0x3F,
  0xD,0x0,0x32,0xD,0x0,0x33,0xD,0x0,
  0x33,0x17,0x2,0xD,0x0,0x1C,0xD,0x0,
  0x32,0x17,0x2,0xD,0x0,0x40,0x40,0xD,
  0x0,0x40,0x41,0x17,0x2,0xD,0x0,0x40,
  0x40,0xD,0x0,0x40,0x41,0x65,0x6E,0x76,
  0x69,0x72,0x6F,0x6E,0x73,0x20,0x6D,0x61,
  0x6B,0x65,0x20,0x63,0x6F,0x6E,0x74,0x65,
  0x78,0x74,0x21,0x20,0x71,0x20,0x71,0x75,
  0x69,0x74,0x20,0x79,0x65,0x73,0x20,0x74,
  0x72,0x75,0x65,0x20,0x6E,0x6F,0x20,0x66,
  0x61,0x6C,0x73,0x65,0x20,0x65,0x71,0x3F,
  0x20,0x65,0x71,0x75,0x61,0x6C,0x3F,0x20,
  0x74,0x61,0x69,0x6C,0x3F,0x20,0x65,0x6D,
  0x70,0x74,0x79,0x3F,0x20,0x63,0x6C,0x6F,
  0x73,0x65,0x20,0x66,0x72,0x65,0x65,0x20,
  0x63,0x6F,0x6E,0x74,0x65,0x78,0x74,0x20,
  0x66,0x75,0x6E,0x63,0x20,0x63,0x68,0x61,
  0x72,0x73,0x65,0x74,0x20,0x65,0x72,0x72,
  0x6F,0x72,0x20,0x6A,0x6F,0x69,0x6E,0x20,
  0x72,0x65,0x6A,0x6F,0x69,0x6E,0x20,0x72,
  0x65,0x70,0x6C,0x61,0x63,0x65,0x20,0x73,
  0x70,0x6C,0x69,0x74,0x2D,0x70,0x61,0x74,
  0x68,0x20,0x74,0x65,0x72,0x6D,0x2D,0x64,
  0x69,0x72,0x20,0x76,0x65,0x72,0x73,0x69,
  0x6F,0x6E,0x20,0x6F,0x73,0x20,0x61,0x72,
  0x63,0x68,0x20,0x62,0x69,0x67,0x2D,0x65,
  0x6E,0x64,0x69,0x61,0x6E,0x20,0x6E,0x6F,
  0x6E,0x65,0x20,0x62,0x20,0x62,0x6C,0x6F,
  0x63,0x6B,0x21,0x20,0x73,0x20,0x62,0x69,
  0x74,0x73,0x65,0x74,0x21,0x20,0x73,0x74,
  0x72,0x69,0x6E,0x67,0x21,0x20,0x6E,0x6F,
  0x2D,0x74,0x72,0x61,0x63,0x65,0x20,0x74,
  0x68,0x72,0x6F,0x77,0x20,0x65,0x72,0x72,
  0x6F,0x72,0x21,0x20,0x61,0x20,0x65,0x69,
  0x74,0x68,0x65,0x72,0x20,0x73,0x65,0x72,
  0x69,0x65,0x73,0x3F,0x20,0x61,0x70,0x70,
  0x65,0x6E,0x64,0x20,0x72,0x65,0x64,0x75,
  0x63,0x65,0x20,0x69,0x66,0x20,0x66,0x69,
  0x72,0x73,0x74,0x20,0x6E,0x65,0x78,0x74,
  0x20,0x73,0x65,0x72,0x69,0x65,0x73,0x20,
  0x70,0x61,0x74,0x20,0x72,0x65,0x70,0x20,
  0x61,0x6C,0x6C,0x20,0x73,0x69,0x7A,0x65,
  0x20,0x70,0x61,0x74,0x68,0x20,0x65,0x6E,
  0x64,0x20,0x63,0x6F,0x70,0x79,0x20,0x74,
  0x6F,0x2D,0x74,0x65,0x78,0x74,0x20,0x72,
  0x65,0x74,0x75,0x72,0x6E,0x20,0x73,0x69,
  0x7A,0x65,0x3F,0x20,0x66,0x20,0x77,0x68,
  0x69,0x6C,0x65,0x20,0x66,0x69,0x6E,0x64,
  0x20,0x6C,0x61,0x73,0x74,0x20,0x2B,0x2B,
  0x20,0x74,0x65,0x72,0x6D,0x69,0x6E,0x61,
  0x74,0x65,0x20,0x64,0x69,0x72,0x20,0x73,
  0x6C,0x69,0x63,0x65,0x20,0x63,0x68,0x61,
  0x6E,0x67,0x65,0x20,0x70,0x61,0x72,0x74,
  0x0,
};
