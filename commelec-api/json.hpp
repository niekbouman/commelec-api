#ifndef MYJSONHPP
#define MYJSONHPP

#include <rapidjson/document.h>

rapidjson::Document readJSONfile(const char *configFile);
rapidjson::Document readJSONstdin();
void writeJSONfile(const char *configFile, rapidjson::Document &d);

int getInt(const rapidjson::Value &d, const char *name);
bool getBool(const rapidjson::Value &d, const char *name);
bool getBool(const rapidjson::Value& d,const char *name,bool defaultVal);
double getDouble(const rapidjson::Value &d, const char *name);
std::string getString(const rapidjson::Value &d, const char *name);
// Helper functions to parse JSON with runtime exceptions

const rapidjson::Value&  getRef(const rapidjson::Value& d,const char *name);

#endif
