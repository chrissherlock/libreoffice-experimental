/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <cppunit/TestAssert.h>

#include <vcl/virdev.hxx>
#include <vcl/svapp.hxx>

#include <FontController.hxx>
#include <svdata.hxx>

using namespace vcl;

// Test wrapper to access protected members of OutputDevice
class TestVirtualDevice : public VirtualDevice
{
public:
    using OutputDevice::GetFontCache;
    using OutputDevice::ResetFontCache;
    using OutputDevice::AcquireScreenFontCache;
};

class FontDelegationTest : public CppUnit::TestFixture
{
public:
    void testCacheAdoption();
    void testScreenCacheAcquisition();

    CPPUNIT_TEST_SUITE(FontDelegationTest);
    CPPUNIT_TEST(testCacheAdoption);
    CPPUNIT_TEST(testScreenCacheAcquisition);
    CPPUNIT_TEST_SUITE_END();
};

// Verifies the Hollywood Principle: Devices can share a single cache instance
// successfully through the delegation interface.
void FontDelegationTest::testCacheAdoption()
{
    ScopedVclPtrInstance<TestVirtualDevice> pDeviceA;
    ScopedVclPtrInstance<TestVirtualDevice> pDeviceB;

    pDeviceA->ResetFontCache();
    ImplFontCache* pOriginalAddr = &pDeviceA->GetFontCache();

    // Device B adopts A's cache.
    // We use a no-op deleter to wrap the address for the delegation call.
    auto pShared = std::shared_ptr<ImplFontCache>(pOriginalAddr, [](ImplFontCache*) {});
    pDeviceB->AdoptSharedFontCache(pShared);

    // Identity check: both devices must point to the same physical cache instance
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Cache adoption failed to maintain identity", pOriginalAddr,
                                 &pDeviceB->GetFontCache());
}

// Verifies that AcquireScreenFontCache correctly links to the global SVData singleton
void FontDelegationTest::testScreenCacheAcquisition()
{
    ScopedVclPtrInstance<TestVirtualDevice> pDevice;
    ImplSVData* pSVData = ImplGetSVData();

    pDevice->AcquireScreenFontCache();

    // The device must now be using the exact pointer stored in the global singleton
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Device failed to acquire the global screen cache",
                                 pSVData->maGDIData.mxScreenFontCache.get(),
                                 &pDevice->GetFontCache());

    CPPUNIT_ASSERT_MESSAGE("IsScreenFontCache() reported incorrect status",
                           pDevice->IsScreenFontCache());
}

CPPUNIT_TEST_SUITE_REGISTRATION(FontDelegationTest);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
