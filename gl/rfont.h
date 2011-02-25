

typedef struct
{
    const char* fontFile;
    const uint8_t* glyphs;
    int faceIndex;
    int pointSize;
    int texW;
    int texH;
}
RFontConfig;


int ur_makeRFontBin( UThread* ut, UCell* res, UIndex binN,
                     const UCell* tex, UIndex rastN );
int ur_makeRFont( UThread*, UCell* res, RFontConfig* );


