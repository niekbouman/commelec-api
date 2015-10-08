#include "json.hpp"

#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include <string>
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"

//#include <unistd.h>

rapidjson::Document _readJSONfile(FILE *fp) {
  using namespace rapidjson;
  char readBuffer[65536];
  FileReadStream is(fp, readBuffer, sizeof(readBuffer));
  Document d;
  d.ParseStream(is);
  fclose(fp);
  return d;
}

rapidjson::Document readJSONfile(const char *configFile) {
  // read JSON from file into a rapidJSON document object
  FILE *fp = fopen(configFile, "rb");

  return _readJSONfile(fp);
}

rapidjson::Document readJSONstdin() {
  // read JSON from stdin
  return _readJSONfile(stdin);
}


void writeJSONfile(const char *configFile, rapidjson::Document& d) {
  // write a rapidJSON document object to a file
  using namespace rapidjson;
  FILE *fp = fopen(configFile, "wb");
  if (fp) {
    char writeBuffer[65536];
    FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
    Writer<FileWriteStream> writer(os);
    d.Accept(writer);
    fclose(fp);
    return;
  }
  throw(errno);
}

// Helper function to parse JSON with runtime exceptions
int getInt(const rapidjson::Value& d,const char *name) {
  auto itr = d.FindMember(name);
  if (itr != d.MemberEnd())
    return itr->value.GetInt();
  throw std::runtime_error( (name+std::string(" field missing in JSON object (datatype: int)")).c_str());
}

double getDouble(const rapidjson::Value& d,const char *name) {
  auto itr = d.FindMember(name);
  if (itr != d.MemberEnd())
    return itr->value.GetDouble();
//throw std::runtime_error(name);

  throw std::runtime_error( (name+std::string(" field missing in JSON object (datatype: double)")).c_str());
}

bool getBool(const rapidjson::Value& d,const char *name) {
  auto itr = d.FindMember(name);
  if (itr != d.MemberEnd())
    return itr->value.GetBool();
  throw std::runtime_error((name+std::string(" field missing in JSON object (datatype: boolean)")).c_str());
}

bool getBool(const rapidjson::Value& d,const char *name,bool defaultVal) {
  auto itr = d.FindMember(name);
  if (itr != d.MemberEnd())
    return itr->value.GetBool();
  else return defaultVal;
}


// Helper function to parse JSON with runtime exceptions
std::string getString(const rapidjson::Value& d,const char *name) {
  auto itr = d.FindMember(name);
  if (itr != d.MemberEnd())
    return itr->value.GetString();
  throw std::runtime_error((name+std::string(" field missing in JSON object (datatype: string)")).c_str());
}

