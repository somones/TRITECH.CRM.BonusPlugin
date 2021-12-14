#pragma once
#pragma warning(disable : 4996)

#include <stdexcept>
#include <regex> 
//#include "C:/Users/arun.gnanaraj/Source/Repos/Tritech.License/json.hpp"

#include <iostream>
#include <windows.h>
#include <memory>
#include <string>
#include "json.hpp"
#include <iomanip>
#include <ctime>
#include <sstream>
#include <time.h>   
#include "encrypt.h"
#include <string>
#include <list> 
#include <functional>

using namespace std;
#define BUFFER_SIZE 128

namespace tritech {
     using nlohmann::json;

     inline json get_untyped(const json& j, const char* property) {
          if (j.find(property) != j.end()) {
               return j.at(property).get<json>();
          }
          return json();
     }

     inline json get_untyped(const json& j, std::string property) {
          return get_untyped(j, property.data());
     }

     class License {
     public:
          License() = default;
          virtual ~License() = default;

     private:
          std::string serial;
          std::string function;
          std::string date_end;
          std::string version;
          std::time_t license_time;
          std::string key = "HHP97yPaabGfLvanKMFnxkc4vudF64H6WycEJ3zqyHBDdDYnRM9p9DdgrVrm7PScK2mPCXzF9R7uzkDcRNvmEj9dMt5pa9Nv9N8p";

     public:
          const std::string& get_serial() const { return serial; }
          std::string& get_mutable_serial() { return serial; }
          void set_serial(const std::string& value) { this->serial = value; }

          const std::string& get_function() const { return function; }
          std::string& get_mutable_function() { return function; }
          void set_function(const std::string& value) { this->function = value; }

          const std::string& get_date_end() const { return date_end; }
          std::string& get_mutable_date_end() { return date_end; }
          void set_date_end(const std::string& value) { 
               this->date_end = value;
               struct std::tm tm;
               std::istringstream ss(date_end);
               ss >> std::get_time(&tm, "%d-%m-%Y %H:%M:%S"); // or just %T in this case %d-%m-%Y %H-%M-%S
               license_time = mktime(&tm);
          }

          const std::string& get_version() const { return version; }
          std::string& get_mutable_version() { return version; }
          void set_version(const std::string& value) { this->version = value; }


          bool getDevcieInfo(const char* cmd, list<string>& resultList) {
               char buffer[BUFFER_SIZE];
               bool ret = false;
               FILE* pipe = _popen(cmd, "r"); //Open the pipe and execute the command 
               if (!pipe)
                    return ret;

               const char* name[20] =  { "UUID","ProcessorId","SerialNumber" };
               int len0 = strlen(name[0]), len1 = strlen(name[1]), len2 = strlen(name[2]);
               bool isOk = false;
               while (!feof(pipe))
               {
                    if (fgets(buffer, BUFFER_SIZE, pipe))
                    {
                         if (strncmp(name[0], buffer, len0) == 0
                              || strncmp(name[1], buffer, len1) == 0
                              || strncmp(name[2], buffer, len2) == 0) // Ability to get information correctly
                         {
                              isOk = true;
                              continue;
                         }
                         if (isOk == false
                              || strcmp("\r\n", buffer) == 0) //Remove windows useless blank lines
                         {
                              continue;
                         }
                         ret = true;
                         resultList.push_back(string(buffer));
                    }
               }
               _pclose(pipe); // close the pipeline 
               return ret;
          }


          string getDeviceFingerPrint() {
               list<string> strList;
               list<string>::iterator it;
               hash<string> str_hash;
               size_t num;
               char tmp[11] = { 0 };

               // If the motherboard UUID exists, use the motherboard UUID to generate the machine fingerprint.
               if (getDevcieInfo("wmic csproduct get UUID", strList)
                    && (*strList.begin()).compare("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF\r\n") != 0)
               {
                    //cout << *strList.begin();
                    //cout << str_hash(*strList.begin()) << endl;
                    num = str_hash(*strList.begin());
                    sprintf(tmp, "%u", num);
                    //cout << string(tmp) << endl;
                    return string(tmp);
               }

               // The motherboard UUID does not exist. Use the CPUID, BIOS serial number, and hard disk serial number to generate the machine fingerprint.
               string otherStr("");
               strList.clear();
               if (getDevcieInfo("wmic cpu get processorid", strList)) {
                    otherStr.append(*strList.begin());
               }
               strList.clear();
               if (getDevcieInfo("wmic bios get serialnumber", strList)) {
                    otherStr.append(*strList.begin());
               }
               strList.clear();
               if (getDevcieInfo("wmic diskdrive get serialnumber", strList)) {
                    string allDiskNum("");
                    // There may be more than one hard disk
                    for (it = strList.begin(); it != strList.end(); it++)
                    {
                         allDiskNum.append(*it);
                    }
                    //cout << allDiskNum ;
                    //cout << str_hash(allDiskNum) << endl;
                    otherStr.append(*strList.begin());
               }
               cout << str_hash(otherStr) << endl;
               //memset(tmp,0,11);
               num = str_hash(otherStr);
               sprintf(tmp, "%u", num);
               //cout << string(tmp) << endl;
               return string(tmp);
          }

          //std::string GetHDDSerialNumber() {
          //     //get a handle to the first physical drive
          //     HANDLE h = CreateFileW(L"\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
          //     if (h == INVALID_HANDLE_VALUE) return {};
          //     //an std::unique_ptr is used to perform cleanup automatically when returning (i.e. to avoid code duplication)
          //     std::unique_ptr<std::remove_pointer<HANDLE>::type, void(*)(HANDLE)> hDevice{ h, [](HANDLE handle) {CloseHandle(handle); } };
          //     //initialize a STORAGE_PROPERTY_QUERY data structure (to be used as input to DeviceIoControl)
          //     STORAGE_PROPERTY_QUERY storagePropertyQuery{};
          //     storagePropertyQuery.PropertyId = StorageDeviceProperty;
          //     storagePropertyQuery.QueryType = PropertyStandardQuery;
          //     //initialize a STORAGE_DESCRIPTOR_HEADER data structure (to be used as output from DeviceIoControl)
          //     STORAGE_DESCRIPTOR_HEADER storageDescriptorHeader{};
          //     //the next call to DeviceIoControl retrieves necessary size (in order to allocate a suitable buffer)
          //     //call DeviceIoControl and return an empty std::string on failure
          //     DWORD dwBytesReturned = 0;
          //     if (!DeviceIoControl(hDevice.get(), IOCTL_STORAGE_QUERY_PROPERTY, &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
          //          &storageDescriptorHeader, sizeof(STORAGE_DESCRIPTOR_HEADER), &dwBytesReturned, NULL))
          //          return {};
          //     //allocate a suitable buffer
          //     const DWORD dwOutBufferSize = storageDescriptorHeader.Size;
          //     std::unique_ptr<BYTE[]> pOutBuffer{ new BYTE[dwOutBufferSize]{} };
          //     //call DeviceIoControl with the allocated buffer
          //     if (!DeviceIoControl(hDevice.get(), IOCTL_STORAGE_QUERY_PROPERTY, &storagePropertyQuery, sizeof(STORAGE_PROPERTY_QUERY),
          //          pOutBuffer.get(), dwOutBufferSize, &dwBytesReturned, NULL))
          //          return {};
          //     //read and return the serial number out of the output buffer
          //     STORAGE_DEVICE_DESCRIPTOR* pDeviceDescriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(pOutBuffer.get());
          //     const DWORD dwSerialNumberOffset = pDeviceDescriptor->SerialNumberOffset;
          //     if (dwSerialNumberOffset == 0) return {};
          //     const char* serialNumber = reinterpret_cast<const char*>(pOutBuffer.get() + dwSerialNumberOffset);
          //     return serialNumber;
          //}

          std::string DecryptKey(std::string licenseKey) {
               //std::string msg = "{\"id\":1,\"method\":\"service.subscribe\",\"params\":[\"myapp/0.1c\", null,\"0.0.0.0\",\"80\"]}"; 
               //std::cout << "  message to send: " << msg << std::endl;
               //std::string encrypted_msg = encrypt(msg, key);
               //std::cout << "encrypted message: " << encrypted_msg << std::endl;
               std::string decrypted_msg = decrypt(licenseKey, key);
               std::cout << "decrypted message : " << decrypted_msg << std::endl; 
               return decrypted_msg;
          }

          std::string GetLicense() {               
               nlohmann::json jsondata = nlohmann::json({ {"serial", this->serial }, {"function",this->function}, {"date-end",this->date_end}, {"version",this->version} });
               std::string msg= jsondata.dump();
               std::cout << "message to send : " << msg << std::endl;
               std::string encrypted_msg = encrypt(msg, key);
               std::cout << "encrypted message : " << encrypted_msg << std::endl;
               return encrypted_msg;
          }


          bool IsValid(std::string code) {
               bool isvalid = true;

               std::time_t current_time = std::time(nullptr);
               
             /*  auto t = std::time(nullptr);
               auto tm = *std::localtime(&t); 
               std::ostringstream oss;
               oss << std::put_time(&tm, "%d-%m-%Y %H:%M:%S");
               auto str = oss.str();*/ 
              
               double  seconds = difftime(license_time,current_time );
               string fingerPrint = getDeviceFingerPrint();
               //string serial = GetHDDSerialNumber();
               if (serial != fingerPrint) { isvalid = false;  }
               if (code != function) { isvalid = false; } 
               if ( seconds<0 ) { isvalid = false; }
               return isvalid;
          }
     };
}


namespace nlohmann {
     void from_json(const json& j, tritech::License& x);
     void to_json(json& j, const tritech::License& x);

     inline void from_json(const json& j, tritech::License& x) {
          x.set_serial(j.at("serial").get<std::string>());
          x.set_function(j.at("function").get<std::string>());
          x.set_date_end(j.at("date-end").get<std::string>());
          x.set_version(j.at("version").get<std::string>());
     }

     inline void to_json(json& j, const tritech::License& x) {
          j = json::object();
          j["serial"] = x.get_serial();
          j["function"] = x.get_function();
          j["date-end"] = x.get_date_end();
          j["version"] = x.get_version();
     }
}
