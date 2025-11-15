#include "core/ocr/ocr_engine_bootstrapper.h"
#include "core/ocr/ocr_engine.h"
#include <gtest/gtest.h>

using namespace toriyomi::ocr;

TEST(OcrEngineFactoryTest, CanCreateTesseractEngine) {
    auto engine = OcrEngineFactory::CreateEngine(OcrEngineType::Tesseract);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->GetEngineName(), "Tesseract");
    EXPECT_FALSE(engine->IsInitialized());
}

TEST(OcrEngineFactoryTest, CanCreatePaddleEngineSkeleton) {
    auto engine = OcrEngineFactory::CreateEngine(OcrEngineType::PaddleOCR);
    ASSERT_NE(engine, nullptr);
    EXPECT_EQ(engine->GetEngineName(), "PaddleOCR");
    EXPECT_FALSE(engine->IsInitialized());
}

TEST(OcrEngineBootstrapperTest, PrefersPaddleByDefault) {
    OcrEngineBootstrapper bootstrapper;
    EXPECT_EQ(bootstrapper.GetPreferredEngine(), OcrEngineType::PaddleOCR);
}

TEST(OcrEngineBootstrapperTest, ReturnsNullWhenTessdataMissing) {
    OcrBootstrapConfig config;
    config.tessdataSearchPaths = {"Z:/definitely/missing/path"};
    config.allowTesseractFallback = false;

    OcrEngineBootstrapper bootstrapper(config);
    bootstrapper.SetPreferredEngine(OcrEngineType::Tesseract);

    auto engine = bootstrapper.CreateAndInitialize();
    EXPECT_EQ(engine, nullptr);
}

TEST(OcrEngineBootstrapperTest, PaddleWithoutModelAlsoFailsGracefully) {
    OcrBootstrapConfig config;
    config.paddleModelDirectory = "";
    config.allowTesseractFallback = false;

    OcrEngineBootstrapper bootstrapper(config);
    bootstrapper.SetPreferredEngine(OcrEngineType::PaddleOCR);

    auto engine = bootstrapper.CreateAndInitialize();
    EXPECT_EQ(engine, nullptr);
}
