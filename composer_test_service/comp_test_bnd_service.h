/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __COMP_TEST_BND_SERVICE_H__
#define __COMP_TEST_BND_SERVICE_H__

#include "ICompTestBndService.h"
#include "composer_test_service.h"

// ----------------------------------------------------------------------------

class CompTestBndService : public BnCompTestBndService {
public:
    ~CompTestBndService() = default;
    static int Init(sdm::ComposerTestService *comp_test_service);
    virtual android::status_t dispatch(uint32_t command, const android::Parcel* data,
                                       android::Parcel* reply);
private:
    explicit CompTestBndService(sdm::ComposerTestService *comp_test_service)
        : comp_test_service_(comp_test_service) { }
    sdm::ComposerTestService *comp_test_service_;
};
#endif // __COMP_TEST_BND_SERVICE_H__
