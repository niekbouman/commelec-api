#ifndef MYJSONHPP
#define MYJSONHPP

#include <rapidjson/document.h>

rapidjson::Document readJSONfile(const char *configFile);
rapidjson::Document readJSONstdin();
void writeJSONfile(const char *configFile, rapidjson::Document &d);

int getInt(rapidjson::Document &d, const char *name);
std::string getString(rapidjson::Document &d, const char *name);
// Helper functions to parse JSON with runtime exceptions

#endif
