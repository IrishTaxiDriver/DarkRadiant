#include "RadiantTest.h"

#include "ishaders.h"

namespace test
{

using MaterialsTest = RadiantTest;

TEST_F(MaterialsTest, MaterialFileInfo)
{
    auto& materialManager = GlobalMaterialManager();

    // Expect our example material definitions in the ShaderLibrary
    EXPECT_TRUE(materialManager.materialExists("textures/orbweaver/drain_grille"));
    EXPECT_TRUE(materialManager.materialExists("models/md5/chars/nobles/noblewoman/noblebottom"));
    EXPECT_TRUE(materialManager.materialExists("tdm_spider_black"));

    // ShaderDefinitions should contain their source file infos
    const auto& drainGrille = materialManager.getMaterialForName("textures/orbweaver/drain_grille");
    EXPECT_EQ(drainGrille->getShaderFileInfo()->name, "example.mtr");
    EXPECT_EQ(drainGrille->getShaderFileInfo()->visibility, vfs::Visibility::NORMAL);

    const auto& nobleTop = materialManager.getMaterialForName("models/md5/chars/nobles/noblewoman/nobletop");
    EXPECT_EQ(nobleTop->getShaderFileInfo()->name, "tdm_ai_nobles.mtr");
    EXPECT_EQ(nobleTop->getShaderFileInfo()->visibility, vfs::Visibility::NORMAL);

    // Visibility should be parsed from assets.lst
    const auto& hiddenTex = materialManager.getMaterialForName("textures/orbweaver/drain_grille_h");
    EXPECT_EQ(hiddenTex->getShaderFileInfo()->name, "hidden.mtr");
    EXPECT_EQ(hiddenTex->getShaderFileInfo()->visibility, vfs::Visibility::HIDDEN);

    // assets.lst visibility applies to the MTR file, and should propagate to
    // all shaders within it
    const auto& hiddenTex2 = materialManager.getMaterialForName("textures/darkmod/another_white");
    EXPECT_EQ(hiddenTex2->getShaderFileInfo()->name, "hidden.mtr");
    EXPECT_EQ(hiddenTex2->getShaderFileInfo()->visibility, vfs::Visibility::HIDDEN);
}

TEST_F(MaterialsTest, MaterialParser)
{
    auto& materialManager = GlobalMaterialManager();

    // All of these materials need to be present
    // variant3 lacks whitespace between its name and {, which caused trouble in #4900
    EXPECT_TRUE(materialManager.materialExists("textures/parsing_test/variant1"));
    EXPECT_TRUE(materialManager.materialExists("textures/parsing_test/variant2"));
    EXPECT_TRUE(materialManager.materialExists("textures/parsing_test/variant3"));
}

TEST_F(MaterialsTest, EnumerateMaterialLayers)
{
    auto material = GlobalMaterialManager().getMaterialForName("tdm_spider_black");
    EXPECT_TRUE(material);

    // Get a list of all layers in the material
    auto layers = material->getAllLayers();
    EXPECT_EQ(layers.size(), 4);

    // First layer is the bump map in this particular material
    EXPECT_EQ(layers.at(0)->getType(), ShaderLayer::BUMP);
    EXPECT_EQ(layers.at(0)->getMapImageFilename(),
              "models/md5/chars/monsters/spider/spider_local");

    // Second layer is the diffuse map
    EXPECT_EQ(layers.at(1)->getType(), ShaderLayer::DIFFUSE);
    EXPECT_EQ(layers.at(1)->getMapImageFilename(),
              "models/md5/chars/monsters/spider_black");

    // Third layer is the specular map
    EXPECT_EQ(layers.at(2)->getType(), ShaderLayer::SPECULAR);
    EXPECT_EQ(layers.at(2)->getMapImageFilename(),
              "models/md5/chars/monsters/spider_s");

    // Final layer is the additive "ambient method" stage
    EXPECT_EQ(layers.at(3)->getType(), ShaderLayer::BLEND);
    EXPECT_EQ(layers.at(3)->getMapImageFilename(),
              "models/md5/chars/monsters/spider_black");
    BlendFunc bf = layers.at(3)->getBlendFunc();
    EXPECT_EQ(bf.src, GL_ONE);
    EXPECT_EQ(bf.dest, GL_ONE);
}

TEST_F(MaterialsTest, IdentifyAmbientLight)
{
    auto ambLight = GlobalMaterialManager().getMaterialForName("lights/ambientLight");
    ASSERT_TRUE(ambLight);
    EXPECT_TRUE(ambLight->isAmbientLight());

    auto pointLight = GlobalMaterialManager().getMaterialForName("lights/defaultPointLight");
    ASSERT_TRUE(pointLight);
    EXPECT_FALSE(pointLight->isAmbientLight());

    auto nonLight = GlobalMaterialManager().getMaterialForName("tdm_spider_black");
    ASSERT_TRUE(nonLight);
    EXPECT_FALSE(nonLight->isAmbientLight());
}

}
