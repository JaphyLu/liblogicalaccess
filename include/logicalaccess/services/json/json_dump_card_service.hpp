#pragma once

#include <logicalaccess/lla_core_api.hpp>
#include <logicalaccess/services/cardservice.hpp>
#include <logicalaccess/services/accesscontrol/formats/format.hpp>
#include <nlohmann/json_fwd.hpp>
#include <logicalaccess/services/accesscontrol/formatinfos.hpp>

namespace logicalaccess {

#define JSON_DUMP_CARDSERVICE "JSONDump"

/**
 * A high level service that read data from a card and return
 * its content.
 *
 * The content to fetch, from format to keys, is described by JSON.
 * Similarly, the output is a JSON string representing the data
 * that have been extracted from the card.
 *
 * The input JSON format is RFID.onl format. Todo better documentation ?
 */
class LLA_CORE_API JsonDumpCardService : public CardService {
  public:
    explicit JsonDumpCardService(const std::shared_ptr<Chip> &chip);

    std::string getCSType() override
    {
        return JSON_DUMP_CARDSERVICE;
    }

    std::string dump(const std::string &json_template);

    std::map<std::string, std::shared_ptr<Format>> formats_;
    std::map<std::string, std::shared_ptr<Key>> keys_;
    std::map<std::string, std::shared_ptr<FormatInfos>> format_infos_;

  private:
    void extract_formats(const nlohmann::json & json);
    void extract_keys(const nlohmann::json & json);
};

}
