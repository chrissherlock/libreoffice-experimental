/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <ClippingController.hxx>
#include <vcl/region.hxx>

namespace vcl
{
class ClippingControllerTest : public test::BootstrapFixture
{
public:
    ClippingControllerTest()
        : BootstrapFixture(true, false)
    {
    }
};

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testInitialState)
{
    ClippingController aController;

    // Ensure controller starts in a "Dirty" but not "Clipped" state
    CPPUNIT_ASSERT_MESSAGE("Controller should start dirty to force first sync",
                           aController.IsDirty());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should not be clipped initially", false,
                                 aController.IsOutputClipped());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should not have active clip", false, aController.HasClipRegion());
}

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testEagerClippedFlag)
{
    ClippingController aController;

    // Set an initial valid clip
    aController.SetClipRegion(vcl::Region(tools::Rectangle(0, 0, 10, 10)));
    CPPUNIT_ASSERT_EQUAL(false, aController.IsOutputClipped());
    CPPUNIT_ASSERT(aController.IsDirty());

    // Intersect with a disjoint region to force an empty region
    aController.IntersectClipRegion(vcl::Region(tools::Rectangle(20, 20, 30, 30)));

    // Verify the "Eager Sync" fix: mbOutputClipped must be true immediately
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Disjoint intersection must set mbOutputClipped immediately", true,
                                 aController.IsOutputClipped());
    CPPUNIT_ASSERT_MESSAGE("Controller must remain dirty after intersection",
                           aController.IsDirty());
}

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testResetNoClip)
{
    ClippingController aController;

    aController.SetClipRegion(vcl::Region(tools::Rectangle(0, 0, 5, 5)));
    aController.SetDirty(false); // Simulate a successful hardware sync

    // Resetting to No Clip
    aController.SetNoClipRegion();

    CPPUNIT_ASSERT_EQUAL_MESSAGE("HasClipRegion should be false after reset", false,
                                 aController.HasClipRegion());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Reset should mark state as dirty", true, aController.IsDirty());
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Reset should ensure output is not clipped", false,
                                 aController.IsOutputClipped());
}

CPPUNIT_TEST_FIXTURE(ClippingControllerTest, testIntersectFromNoClip)
{
    ClippingController aController;

    // Intersecting when mbClipRegion is false should just set the region
    vcl::Region aNewRegion(tools::Rectangle(0, 0, 10, 10));
    aController.IntersectClipRegion(aNewRegion);

    CPPUNIT_ASSERT_EQUAL(true, aController.HasClipRegion());
    CPPUNIT_ASSERT_EQUAL(aNewRegion, aController.GetClipRegion());
}

CPPUNIT_PLUGIN_IMPLEMENT();

} // namespace vcl

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
