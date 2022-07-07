#pragma once

#include <memory>

#include "isound.h"
#include "ifilesystem.h"
#include "DeclarationBase.h"

namespace sound
{

/// Representation of a single sound shader.
class SoundShader final :
	public decl::DeclarationBase<ISoundShader>
{
    // Information we have parsed on demand
    struct ParsedContents;
    mutable std::unique_ptr<ParsedContents> _contents;

public:
	using Ptr = std::shared_ptr<SoundShader>;

	SoundShader(const std::string& name);

    ~SoundShader();

    // ISoundShader implementation
	SoundRadii getRadii() override;
	SoundFileList getSoundFileList() override;
	std::string getModName() const override { return getBlockSyntax().getModName(); }
	const std::string& getDisplayFolder() override;
	std::string getShaderFilePath() const override;
	std::string getDefinition() const override;

protected:
    void parseFromTokens(parser::DefTokeniser& tokeniser) override;
    void onSyntaxBlockAssigned(const decl::DeclarationBlockSyntax& block) override;
};

}
