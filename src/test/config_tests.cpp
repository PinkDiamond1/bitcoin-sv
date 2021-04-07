// Copyright (c) 2016 The Bitcoin Core developers
// Copyright (c) 2019 Bitcoin Association
// Distributed under the Open BSV software license, see the accompanying file LICENSE.

#include "chainparams.h"
#include "config.h"
#include "consensus/consensus.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <string>

BOOST_FIXTURE_TEST_SUITE(config_tests, BasicTestingSetup)

static bool isSetDefaultBlockSizeParamsCalledException(const std::runtime_error &ex) {
    static std::string expectedException("GlobalConfig::SetDefaultBlockSizeParams must be called before accessing block size related parameters");
    return expectedException == ex.what();
}

BOOST_AUTO_TEST_CASE(max_block_size) {
    GlobalConfig config;

    // SetDefaultBlockSizeParams must be called before using config block size parameters
    // otherwise getters rise exceptions
    BOOST_CHECK_EXCEPTION(config.GetMaxBlockSize(), std::runtime_error, isSetDefaultBlockSizeParamsCalledException);
    BOOST_CHECK_EXCEPTION(config.GetMaxGeneratedBlockSize(), std::runtime_error, isSetDefaultBlockSizeParamsCalledException);
    BOOST_CHECK_EXCEPTION(config.GetMaxGeneratedBlockSize(0), std::runtime_error, isSetDefaultBlockSizeParamsCalledException);
    BOOST_CHECK_EXCEPTION(config.GetBlockSizeActivationTime(), std::runtime_error, isSetDefaultBlockSizeParamsCalledException);

    config.SetDefaultBlockSizeParams(Params().GetDefaultBlockSizeParams());

    // Too small.
    std::string err = "";
    BOOST_CHECK(!config.SetMaxBlockSize(1, &err));
    BOOST_CHECK(!err.empty());
    err = "";
    BOOST_CHECK(!config.SetMaxBlockSize(12345, &err));
    BOOST_CHECK(!err.empty());
    BOOST_CHECK(!config.SetMaxBlockSize(LEGACY_MAX_BLOCK_SIZE - 1));
    BOOST_CHECK(!config.SetMaxBlockSize(LEGACY_MAX_BLOCK_SIZE));


    // LEGACY_MAX_BLOCK_SIZE + 1
    err = "";
    BOOST_CHECK(config.SetMaxBlockSize(LEGACY_MAX_BLOCK_SIZE + 1, &err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), LEGACY_MAX_BLOCK_SIZE + 1);

    // 2MB
    BOOST_CHECK(config.SetMaxBlockSize(2 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), 2 * ONE_MEGABYTE);

    // 8MB
    BOOST_CHECK(config.SetMaxBlockSize(8 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), 8 * ONE_MEGABYTE);

    // Invalid size keep config.
    BOOST_CHECK(!config.SetMaxBlockSize(54321));
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), 8 * ONE_MEGABYTE);

    // Setting it back down
    BOOST_CHECK(config.SetMaxBlockSize(7 * ONE_MEGABYTE));
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), 7 * ONE_MEGABYTE);
    BOOST_CHECK(config.SetMaxBlockSize(ONE_MEGABYTE + 1));
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), ONE_MEGABYTE + 1);
}

BOOST_AUTO_TEST_CASE(max_block_size_related_defaults) {

    GlobalConfig config;

    // Make up some dummy parameters taking into account the following rules
    // - Block size should be at least 1000 
    // - generated block size can not be larger than received block size - 1000
    DefaultBlockSizeParams defaultParams {
        // activation time 
        1000,
        // max block size
        6000,
        // max generated block size before activation
        3000,
        // max generated block size after activation
        4000
    };

    config.SetDefaultBlockSizeParams(defaultParams);

    // Make up genesis activation parameters 
    // - Genesis will be activated at block height 100
    int32_t heightActivateGenesis   = 100;
    config.SetGenesisActivationHeight(heightActivateGenesis);

    // Providing defaults should not override anything
    BOOST_CHECK(!config.MaxGeneratedBlockSizeOverridden());

    BOOST_CHECK_EQUAL(config.GetBlockSizeActivationTime(), 1000);
    BOOST_CHECK_EQUAL(config.GetGenesisActivationHeight(), 100);

    // Functions that do not take time parameter should return future data
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), defaultParams.maxBlockSize);
    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(), defaultParams.maxGeneratedBlockSizeAfter);


    ///////////////////
    /// Test with default values - they should change based on activation time
    /////////////////
       
    // Functions that do take time parameter should return old values before activation time
    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(999), defaultParams.maxGeneratedBlockSizeBefore);

    // Functions that do take time parameter should return new values on activation time
    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(1000), defaultParams.maxGeneratedBlockSizeAfter);

    // Functions that do take time parameter should return new value after activation date
    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(1001), defaultParams.maxGeneratedBlockSizeAfter);

    // Override one of the values, the overriden value should be used regardless of time.
    // Minimum allowed received block size is 1 MB, so we use 8 MB
    uint64_t overridenMaxBlockSize { 8 * ONE_MEGABYTE };

    BOOST_CHECK(config.SetMaxBlockSize(overridenMaxBlockSize));
    BOOST_CHECK_EQUAL(config.GetMaxBlockSize(), overridenMaxBlockSize);
    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(999), defaultParams.maxGeneratedBlockSizeBefore);

    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(1000), defaultParams.maxGeneratedBlockSizeAfter);

    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(1001), defaultParams.maxGeneratedBlockSizeAfter);


    // Override the generated block size, which must be smaller than received block size
    uint64_t overridenMagGeneratedBlockSize = overridenMaxBlockSize - ONE_MEGABYTE;

    BOOST_CHECK(config.SetMaxGeneratedBlockSize(overridenMagGeneratedBlockSize));
    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(999), overridenMagGeneratedBlockSize);

    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(1000), overridenMagGeneratedBlockSize);

    BOOST_CHECK_EQUAL(config.GetMaxGeneratedBlockSize(1001), overridenMagGeneratedBlockSize);

}

BOOST_AUTO_TEST_CASE(max_tx_size) {

    GlobalConfig config;
    std::string reason;
    int64_t newMaxTxSizePolicy{ MAX_TX_SIZE_POLICY_BEFORE_GENESIS + 1 };


    // default pre genesis policy tx size
    BOOST_CHECK(config.GetMaxTxSize(false, false) == MAX_TX_SIZE_POLICY_BEFORE_GENESIS);

    // default post genesis policy tx size
    BOOST_CHECK(config.GetMaxTxSize(true, false) == DEFAULT_MAX_TX_SIZE_POLICY_AFTER_GENESIS);

    // default pre genesis consensus tx size
    BOOST_CHECK(config.GetMaxTxSize(false, true) == MAX_TX_SIZE_CONSENSUS_BEFORE_GENESIS);

    // default post genesis consensus tx size
    BOOST_CHECK(config.GetMaxTxSize(true, true) == MAX_TX_SIZE_CONSENSUS_AFTER_GENESIS);


    // can not set policy tx size < pre genesis policy tx size
    BOOST_CHECK(!config.SetMaxTxSizePolicy(MAX_TX_SIZE_POLICY_BEFORE_GENESIS - 1, &reason));

    // can not set policy tx size > post genesis consensus tx size
    BOOST_CHECK(!config.SetMaxTxSizePolicy(MAX_TX_SIZE_CONSENSUS_AFTER_GENESIS + 1, &reason));

    // can not set policy tx size < 0
    BOOST_CHECK(!config.SetMaxTxSizePolicy(- 1, &reason));


    // set new max policy tx size
    BOOST_CHECK(config.SetMaxTxSizePolicy(newMaxTxSizePolicy, &reason));

    // pre genesis policy tx size
    BOOST_CHECK(config.GetMaxTxSize(false, false) == MAX_TX_SIZE_POLICY_BEFORE_GENESIS);

    // post genesis policy tx size
    BOOST_CHECK(config.GetMaxTxSize(true, false) == static_cast<uint64_t>(newMaxTxSizePolicy));


    // set unlimited policy tx size
    BOOST_CHECK(config.SetMaxTxSizePolicy(0, &reason));

    // pre genesis policy tx size
    BOOST_CHECK(config.GetMaxTxSize(false, false) == MAX_TX_SIZE_POLICY_BEFORE_GENESIS);

    // post genesis policy tx size
    BOOST_CHECK(config.GetMaxTxSize(true, false) == MAX_TX_SIZE_CONSENSUS_AFTER_GENESIS);
}

BOOST_AUTO_TEST_CASE(max_bignum_length_policy) {

    GlobalConfig config;
    std::string reason;
    int64_t newMaxScriptNumLengthPolicy{ MAX_SCRIPT_NUM_LENGTH_BEFORE_GENESIS + 1 };

    // default pre genesis policy max length
    BOOST_CHECK(config.GetMaxScriptNumLength(false, false) == MAX_SCRIPT_NUM_LENGTH_BEFORE_GENESIS);

    // default post genesis policy max length
    BOOST_CHECK(config.GetMaxScriptNumLength(true, false) == DEFAULT_SCRIPT_NUM_LENGTH_POLICY_AFTER_GENESIS);

    // default pre genesis consensus max length
    BOOST_CHECK(config.GetMaxScriptNumLength(false, true) == MAX_SCRIPT_NUM_LENGTH_BEFORE_GENESIS);

    // default post genesis consensus max length
    BOOST_CHECK(config.GetMaxScriptNumLength(true, true) == MAX_SCRIPT_NUM_LENGTH_AFTER_GENESIS);

    // can not set script number length policy > post genesis consensus script number length
    BOOST_CHECK(!config.SetMaxScriptNumLengthPolicy(MAX_SCRIPT_NUM_LENGTH_AFTER_GENESIS + 1, &reason));

    // can not set policy script number length < 0
    BOOST_CHECK(!config.SetMaxScriptNumLengthPolicy(-1, &reason));

    // set new max policy script number length
    BOOST_CHECK(config.SetMaxScriptNumLengthPolicy(newMaxScriptNumLengthPolicy, &reason));

    // pre genesis policy script number length
    BOOST_CHECK(config.GetMaxScriptNumLength(false, false) == MAX_SCRIPT_NUM_LENGTH_BEFORE_GENESIS);

    // post genesis policy script number length
    BOOST_CHECK(config.GetMaxScriptNumLength(true, false) == static_cast<uint64_t>(newMaxScriptNumLengthPolicy));

    // set unlimited policy script number length
    BOOST_CHECK(config.SetMaxScriptNumLengthPolicy(0, &reason));

    // pre genesis policy script number length
    BOOST_CHECK(config.GetMaxScriptNumLength(false, false) == MAX_SCRIPT_NUM_LENGTH_BEFORE_GENESIS);

    // post genesis policy script number length
    BOOST_CHECK(config.GetMaxScriptNumLength(true, false) == MAX_SCRIPT_NUM_LENGTH_AFTER_GENESIS);
}


BOOST_AUTO_TEST_CASE(hex_to_array) {
    const std::string hexstr = "0a0b0C0D";//Lower and Upper char should both work
    CMessageHeader::MessageMagic array;
    BOOST_CHECK(HexToArray(hexstr, array)); 
    BOOST_CHECK_EQUAL(array[0],10);
    BOOST_CHECK_EQUAL(array[1],11);
    BOOST_CHECK_EQUAL(array[2],12);
    BOOST_CHECK_EQUAL(array[3],13);
}

BOOST_AUTO_TEST_CASE(chain_params) {
    GlobalConfig config;

    // Global config is consistent with params.
    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::TESTNET);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());

    SelectParams(CBaseChainParams::REGTEST);
    BOOST_CHECK_EQUAL(&Params(), &config.GetChainParams());
}

BOOST_AUTO_TEST_CASE(max_stack_size) {

    std::string reason;

    BOOST_CHECK(testConfig.SetMaxStackMemoryUsage(0, 0));
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, true), INT64_MAX);
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, false), INT64_MAX);

    BOOST_CHECK(testConfig.SetMaxStackMemoryUsage(0, DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS));
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, true), INT64_MAX);
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, false), DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS);

    BOOST_CHECK(!testConfig.SetMaxStackMemoryUsage(1000000, 0, &reason));

    BOOST_CHECK(testConfig.SetMaxStackMemoryUsage(200000000, DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS));
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, true), 200000000);
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, false), DEFAULT_STACK_MEMORY_USAGE_POLICY_AFTER_GENESIS);

    BOOST_CHECK(!testConfig.SetMaxStackMemoryUsage(500, 600, &reason));

    BOOST_CHECK(testConfig.SetMaxStackMemoryUsage(600, 500));
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(false, true), INT64_MAX);
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(false, false), INT64_MAX);
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, true), 600);
    BOOST_CHECK_EQUAL(testConfig.GetMaxStackMemoryUsage(true, false), 500);

    BOOST_CHECK(!testConfig.SetMaxStackMemoryUsage(-1, -2));
}

BOOST_AUTO_TEST_CASE(max_send_queues_size) {

    std::string reason;

    uint64_t testBlockSize = LEGACY_MAX_BLOCK_SIZE + 1;
    gArgs.ForceSetArg("-excessiveblocksize", to_string(testBlockSize));
    BOOST_CHECK(testConfig.SetMaxBlockSize(testBlockSize, &reason));
    BOOST_CHECK_EQUAL(testConfig.GetMaxSendQueuesBytes(), testBlockSize * DEFAULT_FACTOR_MAX_SEND_QUEUES_BYTES);

    uint64_t testFactor = 3;
    testConfig.SetFactorMaxSendQueuesBytes(testFactor);
    BOOST_CHECK_EQUAL(testConfig.GetMaxSendQueuesBytes(), testBlockSize * testFactor);
}

BOOST_AUTO_TEST_CASE(block_download_config)
{
    GlobalConfig config {};
    std::string err {};

    BOOST_CHECK_EQUAL(config.GetBlockStallingMinDownloadSpeed(), DEFAULT_MIN_BLOCK_STALLING_RATE);
    BOOST_CHECK(config.SetBlockStallingMinDownloadSpeed(2 * DEFAULT_MIN_BLOCK_STALLING_RATE, &err));
    BOOST_CHECK_EQUAL(config.GetBlockStallingMinDownloadSpeed(), 2 * DEFAULT_MIN_BLOCK_STALLING_RATE);
    BOOST_CHECK(config.SetBlockStallingMinDownloadSpeed(0, &err));
    BOOST_CHECK(!config.SetBlockStallingMinDownloadSpeed(-1, &err));

    BOOST_CHECK_EQUAL(config.GetBlockStallingTimeout(), DEFAULT_BLOCK_STALLING_TIMEOUT);
    BOOST_CHECK(config.SetBlockStallingTimeout(2 * DEFAULT_BLOCK_STALLING_TIMEOUT, &err));
    BOOST_CHECK_EQUAL(config.GetBlockStallingTimeout(), 2 * DEFAULT_BLOCK_STALLING_TIMEOUT);
    BOOST_CHECK(!config.SetBlockStallingTimeout(0, &err));
    BOOST_CHECK(!config.SetBlockStallingTimeout(-1, &err));

    BOOST_CHECK_EQUAL(config.GetBlockDownloadWindow(), DEFAULT_BLOCK_DOWNLOAD_WINDOW);
    BOOST_CHECK(config.SetBlockDownloadWindow(2 * DEFAULT_BLOCK_DOWNLOAD_WINDOW, &err));
    BOOST_CHECK_EQUAL(config.GetBlockDownloadWindow(), 2 * DEFAULT_BLOCK_DOWNLOAD_WINDOW);
    BOOST_CHECK(!config.SetBlockDownloadWindow(0, &err));
    BOOST_CHECK(!config.SetBlockDownloadWindow(-1, &err));

    BOOST_CHECK_EQUAL(config.GetBlockDownloadSlowFetchTimeout(), DEFAULT_BLOCK_DOWNLOAD_SLOW_FETCH_TIMEOUT);
    BOOST_CHECK(config.SetBlockDownloadSlowFetchTimeout(2 * DEFAULT_BLOCK_DOWNLOAD_SLOW_FETCH_TIMEOUT, &err));
    BOOST_CHECK_EQUAL(config.GetBlockDownloadSlowFetchTimeout(), 2 * DEFAULT_BLOCK_DOWNLOAD_SLOW_FETCH_TIMEOUT);
    BOOST_CHECK(!config.SetBlockDownloadSlowFetchTimeout(0, &err));
    BOOST_CHECK(!config.SetBlockDownloadSlowFetchTimeout(-1, &err));

    BOOST_CHECK_EQUAL(config.GetBlockDownloadMaxParallelFetch(), DEFAULT_MAX_BLOCK_PARALLEL_FETCH);
    BOOST_CHECK(config.SetBlockDownloadMaxParallelFetch(2 * DEFAULT_MAX_BLOCK_PARALLEL_FETCH, &err));
    BOOST_CHECK_EQUAL(config.GetBlockDownloadMaxParallelFetch(), 2 * DEFAULT_MAX_BLOCK_PARALLEL_FETCH);
    BOOST_CHECK(!config.SetBlockDownloadMaxParallelFetch(0, &err));
    BOOST_CHECK(!config.SetBlockDownloadMaxParallelFetch(-1, &err));
}

BOOST_AUTO_TEST_CASE(p2p_config)
{
    GlobalConfig config {};
    std::string err {};

    BOOST_CHECK_EQUAL(config.GetP2PHandshakeTimeout(), DEFAULT_P2P_HANDSHAKE_TIMEOUT_INTERVAL);
    BOOST_CHECK(config.SetP2PHandshakeTimeout(2 * DEFAULT_P2P_HANDSHAKE_TIMEOUT_INTERVAL, &err));
    BOOST_CHECK_EQUAL(config.GetP2PHandshakeTimeout(), 2 * DEFAULT_P2P_HANDSHAKE_TIMEOUT_INTERVAL);
    BOOST_CHECK(!config.SetP2PHandshakeTimeout(0, &err));
    BOOST_CHECK(!config.SetP2PHandshakeTimeout(-1, &err));

    BOOST_CHECK_EQUAL(config.GetStreamSendRateLimit(), Stream::DEFAULT_SEND_RATE_LIMIT);
    BOOST_CHECK(config.SetStreamSendRateLimit(1000, &err));
    BOOST_CHECK_EQUAL(config.GetStreamSendRateLimit(), 1000);
    BOOST_CHECK(config.SetStreamSendRateLimit(0, &err));
    BOOST_CHECK(config.SetStreamSendRateLimit(-1, &err));
    BOOST_CHECK_EQUAL(config.GetStreamSendRateLimit(), -1);
}

BOOST_AUTO_TEST_CASE(disable_BIP30)
{
    GlobalConfig config {};
    std::string err {};

    SelectParams(CBaseChainParams::MAIN);
    BOOST_CHECK(config.SetDisableBIP30Checks(true, &err) == false);
    BOOST_CHECK(err == "Can not change disabling of BIP30 checks on " + config.GetChainParams().NetworkIDString() + " network.");
    BOOST_CHECK(config.GetDisableBIP30Checks() == false);

    for(const auto& networkType: {CBaseChainParams::TESTNET, CBaseChainParams::REGTEST, CBaseChainParams::STN})
    {
        config.Reset();
        SelectParams(networkType);
        BOOST_CHECK(config.GetDisableBIP30Checks() == false);
        BOOST_CHECK(config.SetDisableBIP30Checks(true, &err) == true);
        BOOST_CHECK(config.GetDisableBIP30Checks() == true);
        BOOST_CHECK(config.SetDisableBIP30Checks(false, &err) == true);
        BOOST_CHECK(config.GetDisableBIP30Checks() == false);
    }
}

BOOST_AUTO_TEST_CASE(dust_config_test)
{
    GlobalConfig config {};
    std::string err {};

    BOOST_CHECK(config.SetDustLimitFactor(0, &err));
    BOOST_CHECK_EQUAL(config.GetDustLimitFactor(), 0);
    BOOST_CHECK(config.SetDustLimitFactor(100, &err));
    BOOST_CHECK_EQUAL(config.GetDustLimitFactor(), 100);
    BOOST_CHECK(config.SetDustLimitFactor(200, &err));
    BOOST_CHECK_EQUAL(config.GetDustLimitFactor(), 200);
    BOOST_CHECK(config.SetDustLimitFactor(300, &err));
    BOOST_CHECK_EQUAL(config.GetDustLimitFactor(), 300);

    BOOST_CHECK(!config.SetDustLimitFactor(-1, &err));
    BOOST_CHECK(!config.SetDustLimitFactor(301, &err));
}



BOOST_AUTO_TEST_SUITE_END()
