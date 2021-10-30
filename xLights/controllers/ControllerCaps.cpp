
/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/smeighan/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/smeighan/xLights/blob/master/License.txt
 **************************************************************/

#include "ControllerCaps.h"

#include <wx/xml/xml.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/dir.h>
#include <log4cpp/Category.hh>

#include "../UtilFunctions.h"
#include "../outputs/Controller.h"

#pragma region Static Functions
std::map<std::string, std::map<std::string, std::list<ControllerCaps*>>> ControllerCaps::__controllers;

inline ControllerCaps *FindVariant(std::list<ControllerCaps*> &variants, const std::string &var) {
    for (auto it : variants) {
        if (it->GetVariantName() == var) {
            return it;
        }
    }
    return nullptr;
}

static void merge(std::map<std::string, wxXmlNode *> &abstracts, const std::string &base, wxXmlNode *t) {
    wxXmlNode *baseNode = abstracts[base];
    if (baseNode) {
        for (wxXmlNode* nn = baseNode->GetChildren(); nn != nullptr; nn = nn->GetNext()) {
            if (!DoesXmlNodeExist(t, nn->GetName())) {
                wxXmlNode *newNode = new wxXmlNode(*nn);
                t->AddChild(newNode);
            }
        }
        auto newBase = baseNode->GetAttribute("Base");
        if (newBase != "") {
            merge(abstracts, newBase, t);
        }
    }
}

void ControllerCaps::LoadControllers() {

    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    if (__controllers.size() != 0) return;

    wxString d;
    wxStandardPaths stdp = wxStandardPaths::Get();

#ifndef __WXMSW__
    d = wxStandardPaths::Get().GetResourcesDir() + "/controllers";
#else
    d = wxFileName(stdp.GetExecutablePath()).GetPath() + "/controllers";
#endif

#ifdef _DEBUG
    // in debug look in the master folder
    if (!wxDir::Exists(d)) {
#ifdef __WXMSW__
        d = wxFileName(stdp.GetExecutablePath()).GetPath() + "/../../../controllers";
#endif
    }
#endif

    if (wxDir::Exists(d)) {
        wxDir dir(d);

        wxString filename;
        bool cont = dir.GetFirst(&filename, "*.xcontroller", wxDIR_FILES);
        int count = 0;
        while (cont) {
            count++;
            cont = dir.GetNext(&filename);
        }
        std::vector<wxXmlDocument> docs;
        docs.resize(count);
        filename = "";
        cont = dir.GetFirst(&filename, "*.xcontroller", wxDIR_FILES);
        count = 0;
        while (cont) {
            wxFileName fn(dir.GetNameWithSep() + filename);
            wxXmlDocument doc;
            docs[count].Load(fn.GetFullPath());
            if (!docs[count].IsOk()) {
                wxASSERT(false);
                logger_base.error("Problem loading " + fn.GetFullPath());
            }
            count++;
            cont = dir.GetNext(&filename);
        }
        std::map<std::string, wxXmlNode *> abstracts;
        for (auto &doc : docs) {
            if (doc.IsOk()) {
                for (wxXmlNode* n = doc.GetRoot(); n != nullptr; n = n->GetNext()) {
                    if (n->GetName() == "Vendor") {
                        auto vendor = n->GetAttribute("Name");
                        for (wxXmlNode* nn = n->GetChildren(); nn != nullptr; nn = nn->GetNext()) {
                            if (nn->GetName() == "AbstractVariant") {
                                auto var = nn->GetAttribute("Name");
                                abstracts[vendor + ":" + var] = nn;
                            }
                        }
                    }
                }
            }
        }
        for (auto &doc : docs) {
            if (doc.IsOk()) {
                for (wxXmlNode* n = doc.GetRoot(); n != nullptr; n = n->GetNext()) {
                    if (n->GetName() == "Vendor") {
                        auto vendor = n->GetAttribute("Name");

                        if (__controllers.find(vendor) == end(__controllers)) {
                            __controllers[vendor] = std::map<std::string, std::list<ControllerCaps*>>();
                        }

                        auto& v = __controllers[vendor];

                        for (wxXmlNode* nn = n->GetChildren(); nn != nullptr; nn = nn->GetNext()) {
                            if (nn->GetName() == "Controller") {
                                auto controller = nn->GetAttribute("Name");
                                if (v.find(controller) == v.end()) {
                                    v[controller] = std::list<ControllerCaps*>();
                                }

                                auto& c = v[controller];
                                for (wxXmlNode* nnn = nn->GetChildren(); nnn != nullptr; nnn = nnn->GetNext()) {
                                    if (nnn->GetName() == "Variant") {
                                        auto base = nnn->GetAttribute("Base");
                                        merge(abstracts, base, nnn);
                                        c.push_back(new ControllerCaps(vendor, controller, nnn));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        logger_base.error("Controllers folder not found " + d);
    }
}

void ControllerCaps::UnloadControllers() {

    // delete all the wxXmlNodes
    for (const auto& it : __controllers) {
        for (const auto& it2 : it.second) {
            for (auto it3 : it2.second) {
                delete it3;
            }
        }
    }
    __controllers.clear();
}

std::list<std::string> ControllerCaps::GetVendors(const std::string& type) {

    LoadControllers();
    std::list<std::string> vendors;
    vendors.push_back("");
    for (const auto& it : __controllers) {
        bool done = false;
        for (const auto& it2 : it.second) {
            for (const auto& it3 : it2.second) {
                if (type == CONTROLLER_ETHERNET && it3->SupportsEthernetInputProtols()) {
                    vendors.push_back(it.first);
                    done = true;
                    break;
                }
                else if (type == CONTROLLER_SERIAL && it3->SupportsSerialInputProtols()) {
                    vendors.push_back(it.first);
                    done = true;
                    break;
                }
            }
            if (done) break;
        }
    }
    return vendors;
}

std::list<std::string> ControllerCaps::GetModels(const std::string& type, const std::string& vendor) {

    LoadControllers();
    std::list<std::string> models;
    models.push_back("");

    auto v = __controllers.find(vendor);
    if (v != __controllers.end()) {
        for (const auto& it : v->second) {
            for (const auto& it3 : it.second) {
                if (type == CONTROLLER_ETHERNET && it3->SupportsEthernetInputProtols()) {
                    models.push_back(it.first);
                    break;
                }
                else if (type == CONTROLLER_SERIAL && it3->SupportsSerialInputProtols()) {
                    models.push_back(it.first);
                    break;
                }
            }
        }
    }
    return models;
}

std::list<std::string> ControllerCaps::GetVariants(const std::string& type, const std::string& vendor, const std::string& model) {

    LoadControllers();
    std::list<std::string> versions;

    auto v = __controllers.find(vendor);
    if (v != __controllers.end()) {
        auto m = v->second.find(model);
        if (m != v->second.end()) {
            for (const auto& it : m->second) {
                if (it->GetVariantName() != "") {
                    if (type == CONTROLLER_ETHERNET && it->SupportsEthernetInputProtols()) {
                        versions.push_back(it->GetVariantName());
                    }
                    else if (type == CONTROLLER_SERIAL && it->SupportsSerialInputProtols()) {
                        versions.push_back(it->GetVariantName());
                    }
                }
            }
        }
    }
    if (versions.empty()) {
        versions.push_back("");
    }
    return versions;
}

ControllerCaps* ControllerCaps::GetControllerConfig(const Controller* const controller) {

    if (controller == nullptr) return nullptr;

    return GetControllerConfig(controller->GetVendor(), controller->GetModel(), controller->GetVariant());
}

ControllerCaps* ControllerCaps::GetControllerConfig(const std::string& vendor, const std::string& model, const std::string& variant) {
    LoadControllers();
    std::list<std::string> versions;

    auto v = __controllers.find(vendor);
    if (v != __controllers.end()) {
        auto m = v->second.find(model);
        if (m != v->second.end()) {
            auto f = FindVariant(m->second, variant);
            if (f) return f;
        }
    }
    return nullptr;
}
ControllerCaps* ControllerCaps::GetControllerConfigByID(const std::string& ID) {
    if (ID == "") {
        return nullptr;
    }
    LoadControllers();
    for (auto &v : __controllers) {
        for (auto &m : v.second) {
            for (auto &vr : m.second) {
                if (ID == vr->GetID()) {
                    return vr;
                }
            }
        }
    }
    return nullptr;
}
#pragma endregion

#pragma region Getters and Setters

bool ControllerCaps::SupportsUpload() const {

    return DoesXmlNodeExist(_config, "SupportsUpload") || 
           DoesXmlNodeExist(_config, "SupportsInputOnlyUpload");
}

bool ControllerCaps::SupportsFullxLightsControl() const
{
    return DoesXmlNodeExist(_config, "SupportsFullxLightsControl");
}

bool ControllerCaps::SupportsInputOnlyUpload() const {

    return DoesXmlNodeExist(_config, "SupportsInputOnlyUpload");
}

bool ControllerCaps::NeedsDDPInputUpload() const
{
    return DoesXmlNodeExist(_config, "NeedsDDPInputUpload");
}

bool ControllerCaps::SupportsLEDPanelMatrix() const {

    return DoesXmlNodeExist(_config, "SupportsLEDPanelMatrix");
}
bool ControllerCaps::SupportsVirtualMatrix() const {

    return DoesXmlNodeExist(_config, "SupportsVirtualMatrix");
}

bool ControllerCaps::SupportsVirtualStrings() const {

    return DoesXmlNodeExist(_config, "SupportsVirtualStrings");
}

bool ControllerCaps::SupportsSmartRemotes() const {

    return DoesXmlNodeExist(_config, "SupportsSmartRemotes");
}

bool ControllerCaps::SupportsRemotes() const
{
    return DoesXmlNodeExist(_config, "SupportsRemotes");
}

bool ControllerCaps::SupportsAutoLayout() const {
    return DoesXmlNodeExist(_config, "SupportsAutoLayout");
}

bool ControllerCaps::SupportsAutoUpload() const {
    return DoesXmlNodeExist(_config, "SupportsAutoUpload");
}

bool ControllerCaps::SupportsUniversePerString() const
{
    return DoesXmlNodeExist(_config, "SupportsUniversePerString");
}

bool ControllerCaps::SupportsMultipleSimultaneousOutputProtocols() const {

    return DoesXmlNodeExist(_config, "SupportsMultipleSimultaneousOutputProtocols");
}

bool ControllerCaps::SupportsMultipleSimultaneousInputProtocols() const {

    return DoesXmlNodeExist(_config, "SupportsMultipleSimultaneousInputProtocols");
}

bool ControllerCaps::MergeConsecutiveVirtualStrings() const {

    return DoesXmlNodeExist(_config, "MergeConsecutiveVirtualStrings");
}

bool ControllerCaps::AllInputUniversesMustBeSameSize() const {

    return DoesXmlNodeExist(_config, "AllInputUniversesMustBeSameSize");
}

bool ControllerCaps::UniversesMustBeInNumericalOrder() const {

    return DoesXmlNodeExist(_config, "UniversesMustBeInNumericalOrder");
}

bool ControllerCaps::UniversesMustBeSequential() const {

    return DoesXmlNodeExist(_config, "UniversesMustBeSequential");
}

bool ControllerCaps::NoWebUI() const
{
    return DoesXmlNodeExist(_config, "NoWebUI");
}

bool ControllerCaps::SupportsPixelPortBrightness() const {

    return SupportsPixelPortCommonSettings() || DoesXmlNodeExist(_config, "SupportsPixelPortBrightness");
}

bool ControllerCaps::SupportsPixelPortGamma() const {

    return SupportsPixelPortCommonSettings() || DoesXmlNodeExist(_config, "SupportsPixelPortGamma");
}

bool ControllerCaps::SupportsPixelPortColourOrder() const {

    return SupportsPixelPortCommonSettings() || DoesXmlNodeExist(_config, "SupportsPixelPortColourOrder");
}

bool ControllerCaps::SupportsEthernetInputProtols() const
{
    for (const auto& it : GetInputProtocols()) {
        if (it == "e131" || it == "artnet" || it == "kinet" || it == "zcpp" || it == "ddp" || it == "opc" || it == "xxx ethernet") return true;
    }
    return false;
}

bool ControllerCaps::SupportsSerialInputProtols() const
{
    for (const auto& it : GetInputProtocols()) {
        if (it == "dmx" || it == "lor" || it == "renard" || 
            it == "opendmx" || it == "pixelnet" || it == "open pixelnet" || 
            it == "dlight" || it == "lor optimised" || it == "xxx serial" || it == "ddp-input") return true;
    }
    return false;
}

bool ControllerCaps::NeedsFullUniverseForDMX() const
{
    return DoesXmlNodeExist(_config, "NeedsFullUniverseForSerial");
}

bool ControllerCaps::SupportsPixelPortNullPixels() const {

    return SupportsPixelPortCommonSettings() || DoesXmlNodeExist(_config, "SupportsPixelPortNullPixels");
}

bool ControllerCaps::SupportsPixelPortEndNullPixels() const
{

    return DoesXmlNodeExist(_config, "SupportsPixelPortEndNullPixels");
}

bool ControllerCaps::SupportsPixelPortDirection() const {

    return SupportsPixelPortCommonSettings() || DoesXmlNodeExist(_config, "SupportsPixelPortDirection");
}

bool ControllerCaps::SupportsPixelPortGrouping() const {

    return SupportsPixelPortCommonSettings() || DoesXmlNodeExist(_config, "SupportsPixelPortGrouping");
}

bool ControllerCaps::SupportsTs() const
{
    return DoesXmlNodeExist(_config, "SupportsTs");
}

bool ControllerCaps::SupportsPixelPortCommonSettings() const {

    return DoesXmlNodeExist(_config, "SupportsPixelPortCommonSettings");
}

int ControllerCaps::GetMaxInputE131Universes() const {

    return wxAtoi(GetXmlNodeContent(_config, "MaxInputUniverses"));
}

int ControllerCaps::GetSmartRemoteCount() const
{

    return wxAtoi(GetXmlNodeContent(_config, "SupportsSmartRemotes"));
}

int ControllerCaps::GetMaxPixelPort() const {

    return wxAtoi(GetXmlNodeContent(_config, "MaxPixelPort"));
}

int ControllerCaps::GetMaxSerialPort() const {
    return wxAtoi(GetXmlNodeContent(_config, "MaxSerialPort"));
}
int ControllerCaps::GetMaxVirtualMatrixPort() const {
    //for now, use 1 if supported.  Technially FPP supports unlimitted Virtual Matrices,
    //on one port, but has two HDMI ports on the Pi4 so some of this may need to
    //be adjusted at some point
    return SupportsVirtualMatrix() ? wxAtoi(GetXmlNodeContent(_config, "MaxVirtualMatrixPorts", "1")): 0;
}
int ControllerCaps::GetMaxLEDPanelMatrixPort() const {
    //FPP internally can map multiple matrices onto the panel outputs
    //but none of that is exposed in the FPP UI yet so just use a single
    //matrix at this point
    return SupportsLEDPanelMatrix() ? 1 : 0;
}

int ControllerCaps::GetMaxPixelPortChannels() const {

    return wxAtoi(GetXmlNodeContent(_config, "MaxPixelPortChannels"));
}

int ControllerCaps::GetMaxSerialPortChannels() const {

    return wxAtoi(GetXmlNodeContent(_config, "MaxSerialPortChannels"));
}

int ControllerCaps::GetMaxInputUniverseChannels() const {

    return wxAtoi(GetXmlNodeContent(_config, "MaxInputUniverseChannels", "512"));
}

int ControllerCaps::GetMinInputUniverseChannels() const
{
    return wxAtoi(GetXmlNodeContent(_config, "MinInputUniverseChannels", "1"));
}

int ControllerCaps::GetNumberOfBanks() const
{
    return wxAtoi(GetXmlNodeContent(_config, "NumberOfBanks", "1"));
}

int ControllerCaps::GetBankSize() const
{
    return wxAtoi(GetXmlNodeContent(_config, "BankSize", "16"));
}

int ControllerCaps::GetMaxStartNullPixels() const
{
    return wxAtoi(GetXmlNodeContent(_config, "MaxStartNulls", "-1"));
}

int ControllerCaps::GetMaxEndNullPixels() const
{
    return wxAtoi(GetXmlNodeContent(_config, "MaxEndNulls", "-1"));
}

int ControllerCaps::GetMaxGroupPixels() const
{
    return wxAtoi(GetXmlNodeContent(_config, "MaxGroup", "-1"));
}

int ControllerCaps::GetMinGroupPixels() const
{
    return wxAtoi(GetXmlNodeContent(_config, "MinGroup", "-1"));
}

bool ControllerCaps::IsValidPixelProtocol(const std::string& protocol) const {

    auto pp = GetPixelProtocols();
    auto p = Lower(protocol);

    for (const auto& it : pp) {
        if (Lower(it) == p) return true;
    }
    return false;
}

bool ControllerCaps::IsValidSerialProtocol(const std::string& protocol) const {

    auto pp = GetSerialProtocols();
    auto p = Lower(protocol);

    for (const auto& it : pp) {
        if (Lower(it) == p) return true;
    }
    return false;
}

bool ControllerCaps::IsValidInputProtocol(const std::string& protocol) const {

    auto pp = GetInputProtocols();
    auto p = Lower(protocol);

    for (const auto& it : pp) {
        if (Lower(it) == p) return true;
    }
    return false;
}

bool ControllerCaps::IsSerialController() const
{
    return (!IsValidInputProtocol("e131") && !IsValidInputProtocol("artnet") && !IsValidInputProtocol("ddp") && !IsValidInputProtocol("zcpp") && !IsValidInputProtocol("xxx ethernet") && !IsValidInputProtocol("opc") && !IsValidInputProtocol("kinet"));
}

std::vector<std::string> ControllerCaps::GetInputProtocols() const {

    return GetXmlNodeListContent(_config, "InputProtocols", "Protocol");
}

std::vector<std::string> ControllerCaps::GetPixelProtocols() const {

    return GetXmlNodeListContent(_config, "PixelProtocols", "Protocol");
}

std::vector<std::string> ControllerCaps::GetSerialProtocols() const {

    return GetXmlNodeListContent(_config, "SerialProtocols", "Protocol");
}

std::vector<std::string> ControllerCaps::GetAllProtocols() const
{
    std::vector<std::string> res = GetPixelProtocols();
    auto serial = GetSerialProtocols();
    res.insert(end(res), begin(serial), end(serial));
    if (SupportsVirtualMatrix()) {
        res.push_back("Virtual Matrix");
    }
    if (SupportsLEDPanelMatrix()) {
        res.push_back("LED Panel Matrix");
    }
    return res;
}

std::string ControllerCaps::GetVariantName() const {
    auto name = _config->GetAttribute("Name");
    return name.ToStdString();
}
std::string ControllerCaps::GetID() const {
    auto name = _config->GetAttribute("ID");
    return name.ToStdString();
}

std::vector<std::string> ControllerCaps::GetSmartRemoteTypes() const {
    if (!SupportsSmartRemotes()) {
        return { "" };
    }
    auto types = GetXmlNodeListContent(_config, "SmartRemoteTypes", "Type");
    if (types.size() == 0) {
        types.emplace_back("");
    }
    return types;
}

std::string ControllerCaps::GetCustomPropertyByPath(const std::string name, const std::string& def) const {

    return GetXmlNodeContent(_config, name, def);
}

void ControllerCaps::Dump() const
{
    static log4cpp::Category& logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    logger_base.debug("Controller Capabilities " + _vendor + ":" + _model + ":" + GetVariantName());

    if (SupportsUpload()) logger_base.debug("   Supports upload.");
    if (SupportsInputOnlyUpload()) logger_base.debug("   Supports input only upload.");
    if (SupportsLEDPanelMatrix()) logger_base.debug("   Supports LED panel matrices.");
    if (SupportsVirtualStrings()) logger_base.debug("   Supports virtual strings.");
    if (SupportsSmartRemotes()) {
        logger_base.debug("   Supports smart remotes.");
        logger_base.debug("   Supported smart remotes types:");
        for (auto const& it : GetSmartRemoteTypes()) {
            logger_base.debug("      " + it);
        }
    }
    if (SupportsMultipleSimultaneousOutputProtocols()) logger_base.debug("   Supports multiple simultaneous output protocols.");
    if (AllInputUniversesMustBeSameSize()) logger_base.debug("   All input universes must be the same size.");
    if (UniversesMustBeInNumericalOrder()) logger_base.debug("   All input universes must be in numerical order.");
    logger_base.debug("   Inputs: maximum of %d universes.", GetMaxInputE131Universes());
    logger_base.debug("   Input protocols supported:");
    for (const auto& it : GetInputProtocols()) {
        logger_base.debug("      " + it);
    }
    logger_base.debug("   Pixel ports: %d ports with a maximum of %d channels per port.", GetMaxPixelPort(), GetMaxPixelPortChannels());
    logger_base.debug("   Pixel protocols supported:");
    for (const auto& it : GetPixelProtocols()) {
        logger_base.debug("      " + it);
    }
    logger_base.debug("   Serial ports: %d ports with a maximum of %d channels per port.", GetMaxSerialPort(), GetMaxSerialPortChannels());
    logger_base.debug("   Serial protocols supported:");
    for (const auto& it : GetSerialProtocols()) {
        logger_base.debug("      " + it);
    }
}


#pragma endregion
