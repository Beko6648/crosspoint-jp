#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
class FsFile {
 public:
  std::shared_ptr<std::vector<uint8_t>> bytes;
  size_t pos=0;
  explicit operator bool() const { return static_cast<bool>(bytes); }
  int available() const { return bytes?static_cast<int>(bytes->size()-pos):0; }
  int read(void* out,size_t count) { count=std::min(count,static_cast<size_t>(available())); if(count) std::memcpy(out,bytes->data()+pos,count);pos+=count;return count; }
  size_t write(const uint8_t* data,size_t count) { bytes->insert(bytes->end(),data,data+count);return count; }
  size_t write(uint8_t value) { bytes->push_back(value);return 1; }
  void flush() {} void close() {} bool getWriteError() const { return false; }
};
class TestStorage {
 public:
  std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
  bool exists(const std::string& p) const { return files.count(p); }
  bool remove(const std::string& p) { return files.erase(p); }
  bool rename(const std::string& a,const std::string& b) { if(!exists(a))return false;files[b]=files[a];files.erase(a);return true; }
  bool openFileForRead(const char*,const std::string& p,FsFile& f) { if(!exists(p))return false;f.bytes=files[p];f.pos=0;return true; }
  bool openFileForWrite(const char*,const std::string& p,FsFile& f) { files[p]=std::make_shared<std::vector<uint8_t>>();f.bytes=files[p];f.pos=0;return true; }
};
inline TestStorage Storage;
