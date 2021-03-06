/*
Copyright (c) 2014 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "minko/render/ProgramSignature.hpp"

#include "minko/render/Pass.hpp"
#include "minko/data/Container.hpp"
#include "minko/render/DrawCall.hpp"

using namespace minko;
using namespace minko::render;
using namespace minko::data;

//#define DEACTIVATE_FALLBACK // since beta 1

/*static*/ const uint ProgramSignature::MAX_NUM_BINDINGS = 32;

void
ProgramSignature::build(Pass::Ptr                pass,
                        FormatNameFunction        formatNameFunc,
                        Container::Ptr            targetData,
                        Container::Ptr            rendererData,
                        Container::Ptr            rootData,
                        std::string&            defines,
                        std::list<std::string>&    booleanMacros,
                        std::list<std::string>&    integerMacros,
                        std::list<std::string>&    incorrectIntegerMacros)
{
    _mask = 0;
    _values.clear();
    _values.resize(MAX_NUM_BINDINGS, 0);

    defines.clear();

    booleanMacros.clear();
    integerMacros.clear();
    incorrectIntegerMacros.clear();

    std::unordered_map<std::string, MacroBindingDefault> explicitDefinitions;

    pass->getExplicitDefinitions(explicitDefinitions);

    unsigned int macroId    = 0;
    for (auto& macroBinding : pass->macroBindings())
    {
        const auto&    macroName        = macroBinding.first;
        auto        macroDefault    = macroBinding.second;

        const auto& propertyName    = formatNameFunc(std::get<0>(macroDefault));
        auto        source            = std::get<1>(macroDefault);
        auto        container        = source == BindingSource::TARGET
            ? targetData
            : (source == BindingSource::RENDERER ? rendererData : rootData);

        bool                macroExists;
        bool                isMacroInteger;
        MacroBindingDefault    defaultMacro;

        const auto                    foundExplicitDefIt    = explicitDefinitions.find(macroName);
        if (foundExplicitDefIt == explicitDefinitions.end())
        {
            // no explicit definition
            macroExists        = container->hasProperty(propertyName);
            isMacroInteger    = macroExists && container->propertyHasType<int>(propertyName, true);
            defaultMacro    = std::get<2>(macroBinding.second);
        }
        else
        {
            // explicit definition by the pass
            macroExists        = true;
            defaultMacro    = foundExplicitDefIt->second;
            isMacroInteger    = (defaultMacro.semantic == MacroBindingDefaultValueSemantic::VALUE);

            explicitDefinitions.erase(macroName);
        }

        const auto    defaultMacroExists        = defaultMacro.semantic == MacroBindingDefaultValueSemantic::PROPERTY_EXISTS;
        const bool    isDefaultMacroInteger    = defaultMacro.semantic == MacroBindingDefaultValueSemantic::VALUE;
        const bool    canUseDefaultMacro        = defaultMacroExists || isDefaultMacroInteger;


        if (macroExists || canUseDefaultMacro)
        {
            if (pass->isExplicitlyUndefined(macroName))
                continue;

            // WARNING: we do not support more than 32 macro bindings
            if (macroId == MAX_NUM_BINDINGS)
                throw;

            _mask |= 1 << macroId; // update program signature


            if (isMacroInteger || isDefaultMacroInteger)
            {
                const int    min        = std::get<3>(macroBinding.second);
                const int    max        = std::get<4>(macroBinding.second);

                int            value    = isMacroInteger
                    ? container->get<int>(propertyName)
                    : defaultMacro.value.value;

#ifdef DEACTIVATE_FALLBACK
                value = std::max(min, std::min(max, value));
#endif // DEACTIVATE_FALLBACK

                // update program signature
                _values[macroId] = value;

                if (value < min || value > max)
                {
                    if (macroExists)
                        incorrectIntegerMacros.push_back(macroName);

#ifdef DEACTIVATE_FALLBACK
                    throw;
#endif // DEACTIVATE_FALLBACK
                }
                else
                {
                    defines += "#define " + macroName + " " + std::to_string(value) + "\n";

                    if (macroExists && value > 0) // FIXME
                        integerMacros.push_back(macroName);
                }
            }
            else if (macroExists || defaultMacroExists)
            {
                defines += "#define " + macroName + "\n";

                if (macroExists)
                    booleanMacros.push_back(macroName);
            }
        }
        ++macroId;
    }

    // treat remaining unprocessed explicit macro definitions
    for (auto& macroNameAndDefault : explicitDefinitions)
    {
        // WARNING: we do not support more than 32 macro bindings
        if (macroId == MAX_NUM_BINDINGS)
            throw;

        _mask |= 1 << macroId; // update program signature

        const auto& macroName        = macroNameAndDefault.first;
        const auto& macroDefault    = macroNameAndDefault.second;

        if (macroDefault.semantic ==MacroBindingDefaultValueSemantic::VALUE)
            defines += "#define " + macroName + " " + std::to_string(macroDefault.value.value) + "\n";
        else
            defines += "#define " + macroName + "\n";

        ++macroId;
    }
}

bool
ProgramSignature::operator==(const ProgramSignature& x) const
{
    if (_mask != x._mask)
        return false;

    for (unsigned int i = 0; i < MAX_NUM_BINDINGS; ++i)
    {
        if ((_mask >> i) != 0
            && _values[i] != x._values[i])
            return false;
    }
    return true;
}
