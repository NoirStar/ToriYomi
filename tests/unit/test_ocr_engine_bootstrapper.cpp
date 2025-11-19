#include "core/ocr/ocr_engine_bootstrapper.h"
#include "core/ocr/ocr_engine.h"
#include <gtest/gtest.h>

using namespace toriyomi::ocr;

TEST(OcrEngineFactoryTest, CanCreatePaddleEngine) {
    auto engine = OcrEngineFactory::CreateEngine(OcrEngineType::PaddleOCR);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->GetEngineName(), "PaddleOCR");
    EXPECT_FALSE(engine->IsInitialized());
}

TEST(OcrEngineBootstrapperTest, PrefersPaddleByDefault) {
    OcrEngineBootstrapper bootstrapper;
    EXPECT_EQ(bootstrapper.GetPreferredEngine(), OcrEngineType::PaddleOCR);
}

TEST(OcrEngineBootstrapperTest, MissingModelDirectoryReturnsNull) {
    OcrBootstrapConfig config;
    config.paddleModelDirectory.clear();

    OcrEngineBootstrapper bootstrapper(config);
    bootstrapper.SetPreferredEngine(OcrEngineType::PaddleOCR);

    auto engine = bootstrapper.CreateAndInitialize();
    EXPECT_EQ(engine, nullptr);
}

TEST(OcrEngineBootstrapperTest, SupportsChangingPreferredEngine) {
    OcrEngineBootstrapper bootstrapper;
    bootstrapper.SetPreferredEngine(OcrEngineType::EasyOCR);
    EXPECT_EQ(bootstrapper.GetPreferredEngine(), OcrEngineType::EasyOCR);

    auto engine = bootstrapper.CreateAndInitialize();
    EXPECT_EQ(engine, nullptr);
}
