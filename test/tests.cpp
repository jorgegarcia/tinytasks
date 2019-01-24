#include "gtest/gtest.h"
#include "../include/tinytasks.h"

using namespace tinytasks;

TEST(TinyTasksTest, TestLibVersionNumber)
{
    ASSERT_STREQ(tinytasks::version(), "1.0.0");
}

