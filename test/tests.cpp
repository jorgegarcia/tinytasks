#include "gtest/gtest.h"
#include "../include/tinytasks.h"

using namespace tinytasks;

TEST(TinyTasksTest, TestLibVersionNumber)
{
	ASSERT_STREQ(kTinyTasksLibVersion, "1.0.0");
}
