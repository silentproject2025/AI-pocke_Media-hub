#ifndef _MJPEGCLASS_H_
#define _MJPEGCLASS_H_

#include <Arduino.h>
#include <FS.h>
#include <JPEGDEC.h>

#define READ_BUFFER_SIZE 1024

class MjpegClass
{
public:
  int getWidth() const { return _jpgWidth; }
  int getHeight() const { return _jpgHeight; }
  int getScale() const { return _scale; }

  bool setup(
      Stream *input, uint8_t *mjpeg_buf, int32_t mjpeg_buf_size, JPEG_DRAW_CALLBACK *pfnDraw, bool useBigEndian,
      int x, int y, int widthLimit, int heightLimit)
  {
    _input = input;
    _mjpeg_buf = mjpeg_buf;
    _mjpeg_buf_size = mjpeg_buf_size;
    _pfnDraw = pfnDraw;
    _useBigEndian = useBigEndian;
    _x = x;
    _y = y;
    _widthLimit = widthLimit;
    _heightLimit = heightLimit;
    _inputindex = 0;

    if (!_read_buf)
    {
      _read_buf = (uint8_t *)malloc(READ_BUFFER_SIZE);
    }

    if (!_read_buf)
    {
      return false;
    }

    return true;
  }

  bool readMjpegBuf()
  {
    if (_inputindex == 0)
    {
      _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
      _inputindex += _buf_read;
    }
    _mjpeg_buf_offset = 0;
    int i = 0;
    bool found_FFD8 = false;
    while ((_buf_read > 0) && (!found_FFD8))
    {
      i = 0;
      while ((i < (_buf_read - 1)) && (!found_FFD8))
      {
        if ((_read_buf[i] == 0xFF) && (_read_buf[i + 1] == 0xD8)) // JPEG header
        {
          found_FFD8 = true;
        }
        ++i;
      }
      if (found_FFD8)
      {
        --i;
      }
      else
      {
        _read_buf[0] = _read_buf[_buf_read - 1];
        _buf_read = _input->readBytes(_read_buf + 1, READ_BUFFER_SIZE - 1) + 1;
      }
    }
    uint8_t *_p = _read_buf + i;
    _buf_read -= i;
    bool found_FFD9 = false;
    if (_buf_read > 0)
    {
      i = 3;
      while ((_buf_read > 0) && (!found_FFD9))
      {
        if ((_mjpeg_buf_offset > 0) && (_mjpeg_buf[_mjpeg_buf_offset - 1] == 0xFF) && (_p[0] == 0xD9)) // JPEG trailer
        {
          found_FFD9 = true;
        }
        else
        {
          while ((i < (_buf_read - 1)) && (!found_FFD9))
          {
            if ((_p[i] == 0xFF) && (_p[i + 1] == 0xD9)) // JPEG trailer
            {
              found_FFD9 = true;
              i += 2;
              break;
            }
            ++i;
          }
        }

        if (_mjpeg_buf_offset + i > _mjpeg_buf_size)
        {
          return false;
        }

        memcpy(_mjpeg_buf + _mjpeg_buf_offset, _p, i);
        _mjpeg_buf_offset += i;
        size_t o = _buf_read - i;
        if (o > 0)
        {
          memcpy(_read_buf, _p + i, o);
          _buf_read = _input->readBytes(_read_buf + o, READ_BUFFER_SIZE - o);
          _p = _read_buf;
          _inputindex += _buf_read;
          _buf_read += o;
        }
        else
        {
          _buf_read = _input->readBytes(_read_buf, READ_BUFFER_SIZE);
          _p = _read_buf;
          _inputindex += _buf_read;
        }
        i = 0;
      }
      if (found_FFD9)
      {
        return true;
      }
    }

    return false;
  }

  bool drawJpg()
  {
    _remain = _mjpeg_buf_offset;
    _jpeg.openRAM(_mjpeg_buf, _remain, _pfnDraw);
    if (_scale == -1)
    {
      _jpgWidth = _jpeg.getWidth();
      _jpgHeight = _jpeg.getHeight();
      float ratio = (float)_jpgHeight / _heightLimit;
      if (ratio <= 1)
      {
        _scale = 0;
      }
      else if (ratio <= 2)
      {
        _scale = JPEG_SCALE_HALF;
        _jpgWidth /= 2;
        _jpgHeight /= 2;
      }
      else if (ratio <= 4)
      {
        _scale = JPEG_SCALE_QUARTER;
        _jpgWidth /= 4;
        _jpgHeight /= 4;
      }
      else
      {
        _scale = JPEG_SCALE_EIGHTH;
        _jpgWidth /= 8;
        _jpgHeight /= 8;
      }
      _x = (_jpgWidth > _widthLimit) ? 0 : ((_widthLimit - _jpgWidth) / 2);
      _y = (_heightLimit - _jpgHeight) / 2;
    }
    if (_useBigEndian)
    {
      _jpeg.setPixelType(RGB565_BIG_ENDIAN);
    }
    int iXOff, iYOff;
    iXOff = (_widthLimit - _jpeg.getWidth() / (1 << _scale)) / 2;
    if (iXOff < 0) iXOff = 0;
    iYOff = (_heightLimit - _jpeg.getHeight() / (1 << _scale)) / 2;
    if (iYOff < 0) iYOff = 0;

    _jpeg.decode(iXOff, iYOff, _scale);
    _jpeg.close();

    return true;
  }

private:
  Stream *_input;
  uint8_t *_mjpeg_buf;
  int32_t _mjpeg_buf_size;
  JPEG_DRAW_CALLBACK *_pfnDraw;
  bool _useBigEndian;
  int _x;
  int _y;
  int _widthLimit;
  int _heightLimit;
  int _jpgWidth;
  int _jpgHeight;

  uint8_t *_read_buf = nullptr;
  int32_t _mjpeg_buf_offset = 0;

  JPEGDEC _jpeg;
  int _scale = -1;

  int32_t _inputindex = 0;
  int32_t _buf_read;
  int32_t _remain = 0;
};

#endif // _MJPEGCLASS_H_
