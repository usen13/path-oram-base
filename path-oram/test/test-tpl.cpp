#include "trusted_proxy_layer.hpp"
#include "definitions.h"

#include "gtest/gtest.h"
#include <cmath>
#include <numeric>
#include <openssl/aes.h>

using namespace std;

namespace PathORAM
{
	class UtilityTest : public ::testing::Test
	{
	};
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
