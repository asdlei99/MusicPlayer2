#include "stdafx.h"
#include "TagLabHelper.h"
#include "taglib/mp4file.h"
#include "taglib/mp4coverart.h"
#include "taglib/flacfile.h"
#include "taglib/flacpicture.h"
#include "taglib/mpegfile.h"
#include "Common.h"
#include "FilePathHelper.h"
#include "taglib/attachedpictureframe.h"
#include "taglib/id3v2tag.h"
#include "taglib/apefile.h"
#include "taglib/wavfile.h"
#include "taglib/mpcfile.h"
#include "taglib/opusfile.h"
#include "taglib/wavpackfile.h"
#include "taglib/vorbisfile.h"
#include "taglib/trueaudiofile.h"
#include "taglib/aifffile.h"
#include "taglib/asffile.h"
#include "taglib/tpropertymap.h"
#include "AudioCommon.h"


using namespace TagLib;

#define STR_MP4_COVER_TAG "covr"
#define STR_MP4_LYRICS_TAG "----:com.apple.iTunes:Lyrics"
#define STR_ASF_COVER_TAG "WM/Picture"

//��taglib�е��ַ���ת����wstring���͡�
//����taglib�����з�unicode����ȫ����ΪLatin���봦��������޷���ȷ�������ش���ҳ
//���ｫLatin������ַ��������ش���ҳ����
static wstring TagStringToWstring(const String& str)
{
    wstring result;
    if (str.isLatin1())
        result = CCommon::StrToUnicode(str.to8Bit(), CodeType::ANSI);
    else
        result = str.toWString();
    return result;
}


static void SongInfoToTag(const SongInfo& song_info, Tag* tag)
{
    tag->setTitle(song_info.title);
    tag->setArtist(song_info.artist);
    tag->setAlbum(song_info.album);
    tag->setGenre(song_info.genre);
    tag->setTrack(song_info.track);
    tag->setComment(song_info.comment);
    tag->setYear(_wtoi(song_info.year.c_str()));
}

static void TagToSongInfo(SongInfo& song_info, Tag* tag)
{
    song_info.title = TagStringToWstring(tag->title());
    song_info.artist = TagStringToWstring(tag->artist());
    song_info.album = TagStringToWstring(tag->album());
    song_info.genre = TagStringToWstring(tag->genre());
    if (CCommon::StrIsNumber(song_info.genre))
    {
        int genre_num = _wtoi(song_info.genre.c_str());
        song_info.genre = CAudioCommon::GetGenre(static_cast<BYTE>(genre_num - 1));
    }

    unsigned int year = tag->year();
    song_info.year = (year == 0 ? L"" : std::to_wstring(year));
    song_info.track = tag->track();
    song_info.comment = TagStringToWstring(tag->comment());
}

//���ļ����ݶ�ȡ��ByteVector
static void FileToByteVector(ByteVector& data, const std::wstring& file_path)
{
    std::ifstream file{ file_path, std::ios::binary | std::ios::in };
    if (file.fail())
        return;

    //��ȡ�ļ�����
    file.seekg(0, file.end);
    size_t length = file.tellg();
    file.seekg(0, file.beg);

    data.clear();
    data.resize(static_cast<unsigned int>(length));

    file.read(data.data(), length);

    file.close();
}

int GetPicType(const wstring& mimeType)
{
    int type{ -1 };
    if (mimeType == L"image/jpeg" || mimeType == L"image/jpg")
        type = 0;
    else if (mimeType == L"image/png")
        type = 1;
    else if (mimeType == L"image/gif")
        type = 2;
    else if (mimeType == L"image/bmp")
        type = 3;
    else
        type = -1;
    return type;
}


///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////

CTagLabHelper::CTagLabHelper()
{
}

CTagLabHelper::~CTagLabHelper()
{
}

string CTagLabHelper::GetM4aAlbumCover(const wstring& file_path, int& type)
{
    string cover_contents;
    MP4::File file(file_path.c_str());
    auto tag = file.tag();
    if(tag != nullptr)
    {
        auto cover_item = tag->item(STR_MP4_COVER_TAG).toCoverArtList();
        if (!cover_item.isEmpty())
        {
            const auto& pic_data = cover_item.front().data();
            //��ȡר������
            cover_contents.assign(pic_data.data(), pic_data.size());

            //��ȡ�����ʽ
            switch (cover_item.front().format())
            {
            case MP4::CoverArt::JPEG:
                type = 0;
                break;
            case MP4::CoverArt::PNG:
                type = 1;
                break;
            case MP4::CoverArt::BMP:
                type = 3;
                break;
            case MP4::CoverArt::GIF:
                type = 2;
                break;
            default:
                type = -1;
                break;
            }
        }
    }
    return cover_contents;
}

string CTagLabHelper::GetFlacAlbumCover(const wstring& file_path, int& type)
{
    string cover_contents;
    FLAC::File file(file_path.c_str());
    const auto& cover_list = file.pictureList();
    if (!cover_list.isEmpty())
    {
        auto pic = cover_list.front();
        if (pic != nullptr)
        {
            const auto& pic_data = pic->data();
            //��ȡר������
            cover_contents.assign(pic_data.data(), pic_data.size());

            wstring img_type = pic->mimeType().toCWString();
            type = GetPicType(img_type);
        }
    }
    return cover_contents;
}

string CTagLabHelper::GetMp3AlbumCover(const wstring & file_path, int & type)
{
    string cover_contents;
    MPEG::File file(file_path.c_str());
    auto pic_frame_list = file.ID3v2Tag()->frameListMap()["APIC"];
    if (!pic_frame_list.isEmpty())
    {
        ID3v2::AttachedPictureFrame *frame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(pic_frame_list.front());
        if (frame != nullptr)
        {
            auto pic_data = frame->picture();
            //��ȡר������
            cover_contents.assign(pic_data.data(), pic_data.size());
            wstring img_type = frame->mimeType().toCWString();
            type = GetPicType(img_type);
        }
    }
    return cover_contents;
}

string CTagLabHelper::GetAsfAlbumCover(const wstring& file_path, int& type)
{
    string cover_contents;
    ASF::File file(file_path.c_str());
    auto tag = file.tag();
    if (tag != nullptr)
    {
        ASF::AttributeList attr = tag->attribute("WM/Picture");
        if (!attr.isEmpty())
        {
            ASF::Picture picture = attr.front().toPicture();
            auto pic_data = picture.picture();
            cover_contents.assign(pic_data.data(), pic_data.size());
            wstring img_type = picture.mimeType().toCWString();
            type = GetPicType(img_type);
        }
    }
    return cover_contents;
}

void CTagLabHelper::GetFlacTagInfo(SongInfo& song_info)
{
    FLAC::File file(song_info.file_path.c_str());
    if (file.hasID3v1Tag())
        song_info.tag_type |= T_ID3V1;
    if (file.hasID3v2Tag())
        song_info.tag_type |= T_ID3V2;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetM4aTagInfo(SongInfo& song_info)
{
    MP4::File file(song_info.file_path.c_str());
    if (file.hasMP4Tag())
        song_info.tag_type |= T_MP4;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetMpegTagInfo(SongInfo& song_info)
{
    MPEG::File file(song_info.file_path.c_str());
    if (file.hasID3v1Tag())
        song_info.tag_type |= T_ID3V1;
    if (file.hasID3v2Tag())
        song_info.tag_type |= T_ID3V2;
    if (file.hasAPETag())
        song_info.tag_type |= T_APE;

    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetAsfTagInfo(SongInfo& song_info)
{
    ASF::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetApeTagInfo(SongInfo& song_info)
{
    APE::File file(song_info.file_path.c_str());
    if (file.hasID3v1Tag())
        song_info.tag_type |= T_ID3V1;
    if (file.hasAPETag())
        song_info.tag_type |= T_APE;

    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetWavTagInfo(SongInfo& song_info)
{
    RIFF::WAV::File file(song_info.file_path.c_str());
    if (file.hasID3v2Tag())
        song_info.tag_type |= T_ID3V2;
    if (file.hasInfoTag())
        song_info.tag_type |= T_RIFF;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetOggTagInfo(SongInfo& song_info)
{
    Vorbis::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetMpcTagInfo(SongInfo& song_info)
{
    MPC::File file(song_info.file_path.c_str());
    if (file.hasAPETag())
        song_info.tag_type |= T_APE;
    if (file.hasID3v1Tag())
        song_info.tag_type |= T_ID3V1;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetOpusTagInfo(SongInfo& song_info)
{
    Ogg::Opus::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetWavPackTagInfo(SongInfo& song_info)
{
    WavPack::File file(song_info.file_path.c_str());
    if (file.hasAPETag())
        song_info.tag_type |= T_APE;
    if (file.hasID3v1Tag())
        song_info.tag_type |= T_ID3V1;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetTtaTagInfo(SongInfo& song_info)
{
    TrueAudio::File file(song_info.file_path.c_str());
    if (file.hasID3v1Tag())
        song_info.tag_type |= T_ID3V1;
    if (file.hasID3v2Tag())
        song_info.tag_type |= T_ID3V2;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

void CTagLabHelper::GetAiffTagInfo(SongInfo& song_info)
{
    RIFF::AIFF::File file(song_info.file_path.c_str());
    if (file.hasID3v2Tag())
        song_info.tag_type |= T_ID3V2;
    auto tag = file.tag();
    if (tag != nullptr)
    {
        TagToSongInfo(song_info, tag);
    }
}

wstring CTagLabHelper::GetMpegLyric(const wstring& file_path)
{
    wstring lyrics;
    MPEG::File file(file_path.c_str());
    auto id3v2 = file.ID3v2Tag();
    if (id3v2 != nullptr)
    {
        auto frame_list_map = id3v2->frameListMap();
        auto lyric_frame = frame_list_map["USLT"];
        if (!lyric_frame.isEmpty())
            lyrics = lyric_frame.front()->toString().toWString();
    }
    return lyrics;
}

wstring CTagLabHelper::GetM4aLyric(const wstring& file_path)
{
    wstring lyrics;
    MP4::File file(file_path.c_str());
    auto tag = file.tag();
    if (tag != nullptr)
    {
        auto item_map = file.tag()->itemMap();
        auto lyric_item = item_map[STR_MP4_LYRICS_TAG].toStringList();;
        if (!lyric_item.isEmpty())
            lyrics = lyric_item.front().toWString();
    }
    return lyrics;
}

wstring CTagLabHelper::GetFlacLyric(const wstring& file_path)
{
    wstring lyrics;
    FLAC::File file(file_path.c_str());
    auto properties = file.properties();
    auto lyric_item = properties["LYRICS"];
    if (!lyric_item.isEmpty())
    {
        lyrics = lyric_item.front().toWString();
    }
    return lyrics;
}

wstring CTagLabHelper::GetAsfLyric(const wstring& file_path)
{
    wstring lyrics;
    ASF::File file(file_path.c_str());
    auto properties = file.properties();
    auto lyric_item = properties["LYRICS"];
    if (!lyric_item.isEmpty())
    {
        lyrics = lyric_item.front().toWString();
    }
    return lyrics;
}

bool CTagLabHelper::WriteAudioTag(SongInfo& song_info)
{
    AudioType type = CAudioCommon::GetAudioTypeByFileName(song_info.file_path);
    switch (type)
    {
    case AU_MP3:
        return WriteMpegTag(song_info);
    case AU_WMA_ASF:
        return WriteAsfTag(song_info);
    case AU_OGG:
        return WriteOggTag(song_info);
    case AU_MP4:
        return WriteM4aTag(song_info);
    case AU_APE:
        return WriteApeTag(song_info);
    case AU_AIFF:
        return WriteAiffTag(song_info);
    case AU_FLAC:
        return WriteFlacTag(song_info);
    case AU_WAV:
        return WriteWavTag(song_info);
    case AU_MPC:
        break;
    case AU_DSD:
        break;
    case AU_OPUS:
        return WriteOpusTag(song_info);
    case AU_WV:
        return WriteWavPackTag(song_info);
    case AU_SPX:
        break;
    case AU_TTA:
        return WriteTtaTag(song_info);
    default:
        break;
    }
    return false;
}

bool CTagLabHelper::WriteMp3AlbumCover(const wstring& file_path, const wstring& album_cover_path, bool remove_exist)
{
    MPEG::File file(file_path.c_str());
    if (!file.isValid())
        return false;

    //��ɾ��ר������
    if (remove_exist)
    {
        auto id3v2tag = file.ID3v2Tag();
        if (id3v2tag != nullptr)
        {
            auto pic_frame_list = id3v2tag->frameListMap()["APIC"];
            if (!pic_frame_list.isEmpty())
            {
                for (auto frame : pic_frame_list)
                    id3v2tag->removeFrame(frame);
            }
        }
    }
    if (!album_cover_path.empty())
    {
        //��ȡͼƬ�ļ�
        ByteVector pic_data;
        FileToByteVector(pic_data, album_cover_path);
        //����Ƶ�ļ�д��ͼƬ�ļ�
        ID3v2::AttachedPictureFrame* pic_frame = new ID3v2::AttachedPictureFrame();
        pic_frame->setPicture(pic_data);
        pic_frame->setType(ID3v2::AttachedPictureFrame::FrontCover);
        wstring ext = CFilePathHelper(album_cover_path).GetFileExtension();
        pic_frame->setMimeType(L"image/" + ext);
        auto id3v2tag = file.ID3v2Tag(true);
        id3v2tag->addFrame(pic_frame);
    }
    bool saved = file.save(MPEG::File::ID3v2);
    return saved;
}

bool CTagLabHelper::WriteFlacAlbumCover(const wstring& file_path, const wstring& album_cover_path, bool remove_exist)
{
    FLAC::File file(file_path.c_str());
    if (!file.isValid())
        return false;

    //��ɾ��ר������
    if (remove_exist)
    {
        file.removePictures();
    }

    if (!album_cover_path.empty())
    {
        ByteVector pic_data;
        FileToByteVector(pic_data, album_cover_path);
        FLAC::Picture *newpic = new FLAC::Picture();
        newpic->setType(FLAC::Picture::FrontCover);
        newpic->setData(pic_data);
        wstring ext = CFilePathHelper(album_cover_path).GetFileExtension();
        newpic->setMimeType(L"image/" + ext);
        file.addPicture(newpic);
    }
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteM4aAlbumCover(const wstring& file_path, const wstring& album_cover_path, bool remove_exist /*= true*/)
{
    MP4::File file(file_path.c_str());
    if (!file.isValid())
        return false;

    auto tag = file.tag();
    if (tag == nullptr)
        return false;

    if (remove_exist)
    {
        if (tag->contains(STR_MP4_COVER_TAG))
        {
            tag->removeItem(STR_MP4_COVER_TAG);
        }
    }

    if (!album_cover_path.empty())
    {
        ByteVector pic_data;
        FileToByteVector(pic_data, album_cover_path);
        MP4::CoverArt::Format format = MP4::CoverArt::Format::Unknown;
        wstring ext = CFilePathHelper(album_cover_path).GetFileExtension();
        if (ext == L"jpg" || ext == L"jpeg")
            format = MP4::CoverArt::Format::JPEG;
        else if (ext == L"png")
            format = MP4::CoverArt::Format::PNG;
        else if (ext == L"gif")
            format = MP4::CoverArt::Format::GIF;
        else if (ext == L"bmp")
            format = MP4::CoverArt::Format::BMP;
        MP4::CoverArt cover_item(format, pic_data);

        auto cover_item_list = tag->item(STR_MP4_COVER_TAG).toCoverArtList();
        cover_item_list.append(cover_item);
        tag->setItem(STR_MP4_COVER_TAG, cover_item_list);
    }
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteAsfAlbumCover(const wstring& file_path, const wstring& album_cover_path, bool remove_exist /*= true*/)
{
    ASF::File file(file_path.c_str());
    if (!file.isValid())
        return false;

    auto tag = file.tag();
    if (tag == nullptr)
        return false;

    if (remove_exist)
    {
        if (tag->contains(STR_ASF_COVER_TAG))
        {
            tag->removeItem(STR_ASF_COVER_TAG);
        }
    }

    if (!album_cover_path.empty())
    {
        ByteVector pic_data;
        FileToByteVector(pic_data, album_cover_path);

        ASF::Picture picture;
        wstring str_mine_type = L"image/" + CFilePathHelper(album_cover_path).GetFileExtension();
        picture.setMimeType(str_mine_type);
        picture.setType(ASF::Picture::FrontCover);
        picture.setPicture(pic_data);
        tag->setAttribute(STR_ASF_COVER_TAG, picture);
    }
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteMpegTag(SongInfo & song_info)
{
    MPEG::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    int tags = MPEG::File::ID3v2;
    //if (file.hasID3v1Tag())
    //    tags |= MPEG::File::ID3v1;
    if (file.hasAPETag())
        tags |= MPEG::File::APE;
    bool saved = file.save(tags);
    return saved;
}

bool CTagLabHelper::WriteFlacTag(SongInfo& song_info)
{
    FLAC::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteM4aTag(SongInfo & song_info)
{
    MP4::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteWavTag(SongInfo & song_info)
{
    RIFF::WAV::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteOggTag(SongInfo & song_info)
{
    Vorbis::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteApeTag(SongInfo& song_info)
{
    APE::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteOpusTag(SongInfo & song_info)
{
    Ogg::Opus::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteWavPackTag(SongInfo& song_info)
{
    WavPack::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteTtaTag(SongInfo& song_info)
{
    TrueAudio::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteAiffTag(SongInfo & song_info)
{
    RIFF::AIFF::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::WriteAsfTag(SongInfo & song_info)
{
    ASF::File file(song_info.file_path.c_str());
    auto tag = file.tag();
    SongInfoToTag(song_info, tag);
    bool saved = file.save();
    return saved;
}

bool CTagLabHelper::IsFileTypeTagWriteSupport(const wstring& ext)
{
    wstring _ext = ext;
    CCommon::StringTransform(_ext, false);
    AudioType type = CAudioCommon::GetAudioTypeByFileExtension(_ext);
    return type == AU_MP3 || type == AU_FLAC || type == AU_MP4 || type == AU_WAV || type == AU_OGG || type == AU_APE
        || type == AU_WV || type == AU_AIFF || type == AU_OPUS || type == AU_TTA || type == AU_WMA_ASF;
}

bool CTagLabHelper::IsFileTypeCoverWriteSupport(const wstring& ext)
{
    wstring _ext = ext;
    CCommon::StringTransform(_ext, false);
    AudioType type = CAudioCommon::GetAudioTypeByFileExtension(_ext);
    return type == AU_MP3 || type == AU_FLAC || type == AU_MP4 || type == AU_WMA_ASF;
}

bool CTagLabHelper::WriteAlbumCover(const wstring& file_path, const wstring& album_cover_path)
{
    AudioType type = CAudioCommon::GetAudioTypeByFileName(file_path);
    switch (type)
    {
    case AU_MP3:
        return WriteMp3AlbumCover(file_path, album_cover_path);
    case AU_WMA_ASF:
        return WriteAsfAlbumCover(file_path, album_cover_path);
    case AU_OGG:
        break;
    case AU_MP4:
        return WriteM4aAlbumCover(file_path, album_cover_path);
    case AU_APE:
        break;
    case AU_AIFF:
        break;
    case AU_FLAC:
        return WriteFlacAlbumCover(file_path, album_cover_path);
    case AU_WAV:
        break;
    case AU_MPC:
        break;
    case AU_DSD:
        break;
    case AU_OPUS:
        break;
    case AU_WV:
        break;
    case AU_SPX:
        break;
    case AU_TTA:
        break;
    default:
        break;
    }
    return false;
}