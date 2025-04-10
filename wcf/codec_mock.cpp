#include "codec.h"

DecTime_t SilkDecode(std::vector<uint8_t>& silk, std::vector<uint8_t>& pcm, int32_t sr)
{
    return DecTime_t{0, 0};
}

int Silk2Mp3(std::vector<uint8_t> &silk, std::string mp3path, int sr)
{
    // TODO，因为大模型都需要wav格式的音频数据，转成mp3有点多余，这里先放个空实现
    // 让项目好链接
    return 0;
}
