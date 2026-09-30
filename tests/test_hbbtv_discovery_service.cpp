#include "../service/hbbtv_discovery_service.h"

#include <cassert>
#include <cstring>
#include <type_traits>

namespace
{

void requestChannel(
    VdrWebHbbtvDiscoveryV1 &request,
    const char *channelId)
{
    request = {};
    request.structSize = sizeof(request);
    const std::size_t length = std::strlen(channelId);
    assert(length < VDRWEB_HBBTV_CHANNEL_ID_MAX);
    std::memcpy(request.channelId, channelId, length + 1U);
}

}

int main()
{
    static_assert(
        std::is_standard_layout<VdrWebHbbtvApplicationV1>::value,
        "application ABI must be standard layout");
    static_assert(
        std::is_standard_layout<VdrWebHbbtvDiscoveryV1>::value,
        "discovery ABI must be standard layout");

    const char *channel = "C-1-1051-10301";

    VdrWebHbbtvDiscoveryStore::BeginChannel(channel);

    VdrWebHbbtvDiscoveryV1 empty{};
    requestChannel(empty, channel);
    assert(VdrWebHbbtvDiscoveryStore::Read(empty));
    assert(empty.schemaVersion == VDRWEB_HBBTV_SERVICE_SCHEMA_V1);
    assert(empty.result == VDRWEB_HBBTV_RESULT_NO_APPLICATIONS);
    assert(empty.receiverActive == 1);
    assert(empty.applicationCount == 0);
    const uint64_t emptyRevision = empty.discoveryRevision;
    assert(emptyRevision != 0);

    VdrWebHbbtvDiscoveryStore::Upsert(
        channel,
        7,
        2,
        5,
        "ARD HbbTV",
        "https://example.invalid/",
        "index.html",
        "");

    VdrWebHbbtvDiscoveryV1 discovered{};
    requestChannel(discovered, channel);
    assert(VdrWebHbbtvDiscoveryStore::Read(discovered));
    assert(discovered.result == VDRWEB_HBBTV_RESULT_OK);
    assert(discovered.receiverActive == 1);
    assert(discovered.applicationCount == 1);
    assert(discovered.discoveryRevision > emptyRevision);
    assert(discovered.applications[0].applicationId == 7);
    assert(discovered.applications[0].controlCode == 2);
    assert(discovered.applications[0].priority == 5);
    assert(std::strcmp(discovered.applications[0].name, "ARD HbbTV") == 0);
    assert(std::strcmp(
        discovered.applications[0].urlBase,
        "https://example.invalid/") == 0);
    const uint64_t applicationRevision = discovered.discoveryRevision;

    VdrWebHbbtvDiscoveryStore::Upsert(
        channel,
        7,
        2,
        5,
        "ARD HbbTV",
        "https://example.invalid/",
        "index.html",
        "");

    VdrWebHbbtvDiscoveryV1 duplicate{};
    requestChannel(duplicate, channel);
    assert(VdrWebHbbtvDiscoveryStore::Read(duplicate));
    assert(duplicate.discoveryRevision == applicationRevision);

    VdrWebHbbtvDiscoveryStore::Upsert(
        channel,
        7,
        2,
        6,
        "ARD HbbTV",
        "https://example.invalid/",
        "index.html",
        "");

    VdrWebHbbtvDiscoveryV1 changed{};
    requestChannel(changed, channel);
    assert(VdrWebHbbtvDiscoveryStore::Read(changed));
    assert(changed.discoveryRevision > applicationRevision);
    assert(changed.applications[0].priority == 6);

    VdrWebHbbtvDiscoveryV1 mismatch{};
    requestChannel(mismatch, "C-1-1051-99999");
    assert(VdrWebHbbtvDiscoveryStore::Read(mismatch));
    assert(mismatch.result == VDRWEB_HBBTV_RESULT_CHANNEL_MISMATCH);

    VdrWebHbbtvDiscoveryStore::EndChannel();

    VdrWebHbbtvDiscoveryV1 inactive{};
    requestChannel(inactive, channel);
    assert(VdrWebHbbtvDiscoveryStore::Read(inactive));
    assert(inactive.result == VDRWEB_HBBTV_RESULT_RECEIVER_INACTIVE);
    assert(inactive.receiverActive == 0);
    assert(inactive.applicationCount == 0);

    VdrWebHbbtvDiscoveryV1 invalid{};
    invalid.structSize = 1;
    assert(VdrWebHbbtvDiscoveryStore::Read(invalid));
    assert(invalid.result == VDRWEB_HBBTV_RESULT_INVALID_REQUEST);

    return 0;
}
