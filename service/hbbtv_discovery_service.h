#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <mutex>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

// Read-only VDR service for HbbTV application discovery.
#define VDRWEB_SERVICE_HBBTV_DISCOVERY_V1 "VdrWeb::HbbtvDiscovery-v1"

#define VDRWEB_HBBTV_SERVICE_SCHEMA_V1 1U
#define VDRWEB_HBBTV_CHANNEL_ID_MAX 64U
#define VDRWEB_HBBTV_APPLICATION_NAME_MAX 256U
#define VDRWEB_HBBTV_URL_BASE_MAX 1024U
#define VDRWEB_HBBTV_URL_LOCATION_MAX 512U
#define VDRWEB_HBBTV_URL_EXTENSION_MAX 512U
#define VDRWEB_HBBTV_MAX_APPLICATIONS 16U

enum VdrWebHbbtvDiscoveryResultV1 {
    VDRWEB_HBBTV_RESULT_OK = 0,
    VDRWEB_HBBTV_RESULT_INVALID_REQUEST = 1,
    VDRWEB_HBBTV_RESULT_NO_LIVE_SERVICE = 2,
    VDRWEB_HBBTV_RESULT_CHANNEL_MISMATCH = 3,
    VDRWEB_HBBTV_RESULT_NO_APPLICATIONS = 4,
    VDRWEB_HBBTV_RESULT_RECEIVER_INACTIVE = 5
};

struct VdrWebHbbtvApplicationV1 {
    uint32_t applicationId;
    uint8_t controlCode;
    uint8_t priority;
    uint8_t reserved[2];
    char name[VDRWEB_HBBTV_APPLICATION_NAME_MAX];
    char urlBase[VDRWEB_HBBTV_URL_BASE_MAX];
    char urlLocation[VDRWEB_HBBTV_URL_LOCATION_MAX];
    char urlExtension[VDRWEB_HBBTV_URL_EXTENSION_MAX];
};

struct VdrWebHbbtvDiscoveryV1 {
    uint32_t structSize;

    // Request: expected current VDR channel id.
    char channelId[VDRWEB_HBBTV_CHANNEL_ID_MAX];

    // Response.
    uint32_t schemaVersion;
    uint8_t result;
    uint8_t receiverActive;
    uint16_t applicationCount;
    uint64_t discoveryRevision;
    uint64_t observedAt;
    VdrWebHbbtvApplicationV1 applications[VDRWEB_HBBTV_MAX_APPLICATIONS];
};

class VdrWebHbbtvDiscoveryStore final {
private:
    struct Application {
        uint32_t applicationId = 0;
        uint8_t controlCode = 0;
        uint8_t priority = 0;
        std::string name;
        std::string urlBase;
        std::string urlLocation;
        std::string urlExtension;

        bool SamePayload(const Application &other) const {
            return applicationId == other.applicationId &&
                controlCode == other.controlCode &&
                priority == other.priority &&
                name == other.name &&
                urlBase == other.urlBase &&
                urlLocation == other.urlLocation &&
                urlExtension == other.urlExtension;
        }
    };

    struct State {
        std::mutex mutex;
        std::string channelId;
        bool receiverActive = false;
        uint64_t revision = 1;
        uint64_t observedAt = 0;
        std::vector<Application> applications;
    };

    static State &GetState() {
        static State state;
        return state;
    }

    static uint64_t Now() {
        const std::time_t current = std::time(nullptr);
        return current > 0 ? static_cast<uint64_t>(current) : 0;
    }

    static void Touch(State &state) {
        ++state.revision;
        if (state.revision == 0)
            state.revision = 1;
        state.observedAt = Now();
    }

    static std::string Bounded(
        const char *value,
        std::size_t maximumLength)
    {
        if (value == nullptr)
            return {};

        std::size_t length = 0;
        while (length < maximumLength && value[length] != '\0')
            ++length;
        return std::string(value, length);
    }

    static bool RequestChannel(
        const char *value,
        std::size_t capacity,
        std::string &channelId)
    {
        if (value == nullptr || capacity == 0)
            return false;

        std::size_t length = 0;
        while (length < capacity && value[length] != '\0')
            ++length;

        if (length == 0 || length == capacity)
            return false;

        channelId.assign(value, length);
        return true;
    }

    template <std::size_t N>
    static void Copy(char (&target)[N], const std::string &value)
    {
        static_assert(N > 0, "bounded service field");
        const std::size_t length = std::min(value.size(), N - 1);
        std::memcpy(target, value.data(), length);
        target[length] = '\0';
    }

public:
    static void BeginChannel(const std::string &channelId) {
        State &state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);

        const std::string bounded = Bounded(
            channelId.c_str(),
            VDRWEB_HBBTV_CHANNEL_ID_MAX - 1U);

        if (state.receiverActive &&
            state.channelId == bounded &&
            state.applications.empty())
        {
            return;
        }

        state.channelId = bounded;
        state.receiverActive = !state.channelId.empty();
        state.applications.clear();
        Touch(state);
    }

    static void EndChannel() {
        State &state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);

        if (!state.receiverActive && state.applications.empty())
            return;

        state.receiverActive = false;
        state.applications.clear();
        Touch(state);
    }

    static void Upsert(
        const std::string &channelId,
        uint32_t applicationId,
        uint8_t controlCode,
        uint8_t priority,
        const char *name,
        const char *urlBase,
        const char *urlLocation,
        const char *urlExtension)
    {
        if (channelId.empty())
            return;

        Application candidate;
        candidate.applicationId = applicationId;
        candidate.controlCode = controlCode;
        candidate.priority = priority;
        candidate.name = Bounded(
            name,
            VDRWEB_HBBTV_APPLICATION_NAME_MAX - 1U);
        candidate.urlBase = Bounded(
            urlBase,
            VDRWEB_HBBTV_URL_BASE_MAX - 1U);
        candidate.urlLocation = Bounded(
            urlLocation,
            VDRWEB_HBBTV_URL_LOCATION_MAX - 1U);
        candidate.urlExtension = Bounded(
            urlExtension,
            VDRWEB_HBBTV_URL_EXTENSION_MAX - 1U);

        State &state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);

        if (!state.receiverActive || state.channelId != channelId)
            return;

        const auto existing = std::find_if(
            state.applications.begin(),
            state.applications.end(),
            [&](const Application &application) {
                return application.applicationId == applicationId &&
                    application.controlCode == controlCode;
            });

        if (existing != state.applications.end()) {
            if (existing->SamePayload(candidate))
                return;

            *existing = std::move(candidate);
            Touch(state);
            return;
        }

        if (state.applications.size() >= VDRWEB_HBBTV_MAX_APPLICATIONS)
            return;

        state.applications.push_back(std::move(candidate));
        Touch(state);
    }

    static bool Read(VdrWebHbbtvDiscoveryV1 &response) {
        std::string requestedChannel;
        const bool validRequest =
            response.structSize == sizeof(response) &&
            RequestChannel(
                response.channelId,
                VDRWEB_HBBTV_CHANNEL_ID_MAX,
                requestedChannel);

        std::memset(&response, 0, sizeof(response));
        response.structSize = sizeof(response);
        response.schemaVersion = VDRWEB_HBBTV_SERVICE_SCHEMA_V1;

        State &state = GetState();
        std::lock_guard<std::mutex> lock(state.mutex);

        response.discoveryRevision = state.revision;
        response.observedAt = state.observedAt;
        response.receiverActive = state.receiverActive ? 1U : 0U;

        if (!validRequest) {
            response.result = VDRWEB_HBBTV_RESULT_INVALID_REQUEST;
            return true;
        }

        if (state.channelId.empty()) {
            response.result = VDRWEB_HBBTV_RESULT_NO_LIVE_SERVICE;
            return true;
        }

        if (requestedChannel != state.channelId) {
            response.result = VDRWEB_HBBTV_RESULT_CHANNEL_MISMATCH;
            return true;
        }

        if (!state.receiverActive) {
            response.result = VDRWEB_HBBTV_RESULT_RECEIVER_INACTIVE;
            return true;
        }

        if (state.applications.empty()) {
            response.result = VDRWEB_HBBTV_RESULT_NO_APPLICATIONS;
            return true;
        }

        response.result = VDRWEB_HBBTV_RESULT_OK;
        response.applicationCount =
            static_cast<uint16_t>(state.applications.size());

        for (std::size_t index = 0;
             index < state.applications.size();
             ++index)
        {
            const Application &source = state.applications[index];
            VdrWebHbbtvApplicationV1 &target = response.applications[index];

            target.applicationId = source.applicationId;
            target.controlCode = source.controlCode;
            target.priority = source.priority;
            Copy(target.name, source.name);
            Copy(target.urlBase, source.urlBase);
            Copy(target.urlLocation, source.urlLocation);
            Copy(target.urlExtension, source.urlExtension);
        }

        return true;
    }
};
