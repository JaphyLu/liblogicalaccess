#include <logicalaccess/myexception.hpp>
#include <logicalaccess/services/accesscontrol/formats/format.hpp>
#include <logicalaccess/services/accesscontrol/formats/wiegand26format.hpp>
#include <logicalaccess/services/accesscontrol/formats/wiegand34format.hpp>
#include <logicalaccess/services/accesscontrol/formats/wiegand37format.hpp>
#include <logicalaccess/services/accesscontrol/formats/wiegand34withfacilityformat.hpp>
#include <logicalaccess/services/accesscontrol/formats/wiegand37withfacilityformat.hpp>
#include <logicalaccess/services/accesscontrol/formats/customformat/customformat.hpp>
#include <logicalaccess/services/accesscontrol/formats/customformat/stringdatafield.hpp>
#include <logicalaccess/services/accesscontrol/formats/customformat/numberdatafield.hpp>
#include <logicalaccess/services/accesscontrol/formats/customformat/binarydatafield.hpp>
#include <logicalaccess/bufferhelper.hpp>
#include <logicalaccess/plugins/cards/desfire/desfirekey.hpp>
#include <logicalaccess/cards/samkeystorage.hpp>
#include <logicalaccess/plugins/cards/desfire/desfirelocation.hpp>
#include <logicalaccess/plugins/cards/desfire/desfireaccessinfo.hpp>
#include <logicalaccess/services/accesscontrol/formatinfos.hpp>
#include <logicalaccess/services/storage/storagecardservice.hpp>
#include <logicalaccess/services/accesscontrol/accesscontrolcardservice.hpp>
#include "logicalaccess/services/json/json_dump_card_service.hpp"
#include "nlohmann/json.hpp"

namespace logicalaccess
{

JsonDumpCardService::JsonDumpCardService(const std::shared_ptr<Chip> &chip)
    : CardService(chip, CardServiceType::CST_JSON_DUMP)
{
}

std::shared_ptr<Format> createFormat(const nlohmann::json &encodingFormat)
{
    const std::map<std::string, std::shared_ptr<Format>> formatType = {
        {"Wiegand-26", std::make_shared<Wiegand26Format>()},
        {"Wiegand-34", std::make_shared<Wiegand34Format>()},
        {"Wiegand-34-WithFacility", std::make_shared<Wiegand34WithFacilityFormat>()},
        {"Wiegand-37", std::make_shared<Wiegand37Format>()},
        {"Wiegand-37-WithFacility", std::make_shared<Wiegand37WithFacilityFormat>()}};

    // Get standard format
    if (formatType.count(encodingFormat.at("name").get<std::string>()))
        return formatType.at(encodingFormat.at("name").get<std::string>());

    // It is a custom format
    unsigned int offset = 0;
    auto customFormat   = std::make_shared<CustomFormat>();
    customFormat->setName(encodingFormat.at("name").get<std::string>());
    std::vector<std::shared_ptr<DataField>> fields;
    for (const auto &field : encodingFormat.at("fields"))
    {
        std::shared_ptr<DataField> dataField;
        if (field.at("type").get<std::string>() == "FIELD_TYPE_STRING")
        {
            dataField = std::make_shared<StringDataField>();
            std::dynamic_pointer_cast<StringDataField>(dataField)->setDataLength(
                field.at("len").get<int>());
            std::dynamic_pointer_cast<StringDataField>(dataField)->setValue(
                field.at("default_value").get<std::string>());
            std::dynamic_pointer_cast<StringDataField>(dataField)->setIsIdentifier(
                field.at("is_identifier").get<bool>());
        }
        else if (field.at("type").get<std::string>() == "FIELD_TYPE_NUMBER")
        {
            dataField = std::make_shared<NumberDataField>();
            std::dynamic_pointer_cast<NumberDataField>(dataField)->setDataLength(
                field.at("len").get<int>());
            int numb;
            std::istringstream(field.at("default_value").get<std::string>()) >> numb;
            std::dynamic_pointer_cast<NumberDataField>(dataField)->setValue(numb);
            std::dynamic_pointer_cast<NumberDataField>(dataField)->setIsIdentifier(
                field.at("is_identifier").get<bool>());
        }
        else if (field.at("type").get<std::string>() == "FIELD_TYPE_BINARY")
        {
            dataField = std::make_shared<BinaryDataField>();
            auto data =
                BufferHelper::fromHexString(field.at("default_value").get<std::string>());
            std::dynamic_pointer_cast<BinaryDataField>(dataField)->setValue(data);
            std::dynamic_pointer_cast<BinaryDataField>(dataField)->setDataLength(
                field.at("len").get<int>());
            std::dynamic_pointer_cast<BinaryDataField>(dataField)->setIsIdentifier(
                field.at("is_identifier").get<bool>());
        }
        dataField->setName(field.at("name").get<std::string>());
        dataField->setPosition(offset);
        offset += field.at("len").get<int>();
        fields.push_back(dataField);
    }

    customFormat->setFieldList(fields);
    return customFormat;
}

void JsonDumpCardService::extract_formats(const nlohmann::json &json)
{
    try
    {
        for (const auto &format_element : json.at("formats").items())
        {
            auto fmt                       = createFormat(format_element.value());
            formats_[format_element.key()] = fmt;
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        std::stringstream error;
        error << "JSON error when attempting to parse formats: " << e.what();
        throw LibLogicalAccessException(error.str());
    }
}

std::string convert_to_spaced_key(std::string key)
{
    std::string newkeystring;
    for (size_t x = 0; x < key.length(); ++x)
    {
        newkeystring += key[x];
        if (x % 2 != 0 && x + 1 < key.length())
            newkeystring += " ";
    }
    return newkeystring;
}

// todo: make virtual and let each impl create keys for its card type ?
std::shared_ptr<logicalaccess::DESFireKey> getDESFireKey(const nlohmann::json &keyJson)
{
    std::string newkeystring = convert_to_spaced_key(keyJson.at("key_value"));

    const std::map<std::string, logicalaccess::DESFireKeyType> keyType = {
        {"AES", logicalaccess::DESFireKeyType::DF_KEY_AES},
        {"3K3DES", logicalaccess::DESFireKeyType::DF_KEY_3K3DES},
        {"DES", logicalaccess::DESFireKeyType::DF_KEY_DES}};

    auto key = std::make_shared<logicalaccess::DESFireKey>();
    if (keyJson.at("is_sam").get<bool>())
    {
        auto sst = std::make_shared<logicalaccess::SAMKeyStorage>();
        sst->setKeySlot(keyJson.at("sam_key_number"));
        key->setKeyStorage(sst);
    }
    else
    {
        key = std::make_shared<logicalaccess::DESFireKey>(newkeystring);
    }
    key->setKeyVersion(keyJson.at("key_version"));
    key->setKeyType(keyType.at(keyJson.at("key_type")));

    return key;
}

void JsonDumpCardService::extract_keys(const nlohmann::json &json)
{
    try
    {
        for (const auto &key_element : json.at("keyValues").items())
        {
            auto key                 = getDESFireKey(key_element.value());
            keys_[key_element.key()] = key;
        }
    }
    catch (const nlohmann::json::exception &e)
    {
        std::stringstream error;
        error << "JSON error when attempting to parse keys: " << e.what();
        throw LibLogicalAccessException(error.str());
    }
}

// for a single file / format read.
nlohmann::json result_to_json(const std::shared_ptr<logicalaccess::Format> &format)
{
    nlohmann::json json_result;
    for (const auto &field : format->getFieldList())
    {
        if (std::dynamic_pointer_cast<logicalaccess::NumberDataField>(field))
        {
            auto ndf = std::dynamic_pointer_cast<logicalaccess::NumberDataField>(field);
            json_result[field->getName()]["type"]         = "NUMBER";
            json_result[field->getName()]["value"]        = ndf->getValue();
            json_result[field->getName()]["isIdentifier"] = ndf->getIsIdentifier();
        }
        else if (std::dynamic_pointer_cast<logicalaccess::StringDataField>(field))
        {
            auto sdf = std::dynamic_pointer_cast<logicalaccess::StringDataField>(field);
            json_result[field->getName()]["type"]         = "STRING";
            json_result[field->getName()]["value"]        = sdf->getValue();
            json_result[field->getName()]["isIdentifier"] = sdf->getIsIdentifier();
        }
        else if (std::dynamic_pointer_cast<logicalaccess::BinaryDataField>(field))
        {
            auto bdf = std::dynamic_pointer_cast<logicalaccess::BinaryDataField>(field);
            json_result[field->getName()]["type"] = "BINARY";
            auto data                             = bdf->getValue();

            // todo image base 64
            // std::string dataString = base64_encode(data.data(), data.size());
            json_result[field->getName()]["value"]        = "TODO";
            json_result[field->getName()]["isIdentifier"] = bdf->getIsIdentifier();
        }
    }

    return json_result;
}

std::string JsonDumpCardService::dump(const std::string &json_template)
{
    nlohmann::json json = nlohmann::json::parse(json_template);

    extract_formats(json);
    extract_keys(json);

    for (const auto &app : json.at("applications"))
    {
        std::vector<std::shared_ptr<DESFireAccessInfo>> ais;

        for (const auto &file : app.at("files"))
        {
            auto fi = std::make_shared<FormatInfos>();

            auto location = std::make_shared<DESFireLocation>();
            location->aid = std::stoi(app.at("appid").get<std::string>(), nullptr, 16);

            // todo: Maybe not hardcoded ?
            location->securityLevel = EncryptionMode::CM_ENCRYPT;
            location->file          = file.at("fileno");

            auto ai       = std::make_shared<DESFireAccessInfo>();
            ai->readKeyno = file.at("read_key");
            ais.push_back(ai);

            fi->setAiToUse(ai);
            fi->setLocation(location);
            fi->setFormat(formats_.at(file.at("format")));

            std::stringstream tree;
            tree << app.at("name") << file.at("fileno");

            if (format_infos_.find(tree.str()) != format_infos_.end())
                throw LibLogicalAccessException(tree.str() + "Already exists.");
            format_infos_[tree.str()] = fi;
        }

        // Then assign the access info read key to a real key object.
        for (const auto &ai : ais)
        {
            for (const auto &key : app.at("keys"))
            {
                if (ai->readKeyno == key.at("key_no"))
                {
                    ai->readKey =
                        std::dynamic_pointer_cast<DESFireKey>(keys_.at(key.at("key")));
                }
            }
        }
    }

    nlohmann::json json_result;
    auto acs = getChip()->getService<AccessControlCardService>();
    for (const auto &format_info : format_infos_)
    {
        // todo fix for repeated.
        auto result = acs->readFormat(format_info.second->getFormat(),
                                      format_info.second->getLocation(),
                                      format_info.second->getAiToUse());

        json_result[format_info.first] = result_to_json(result);
    }
    return json_result.dump(4);
}
}
