/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

namespace FontValues
{
    static float limitFontHeight (const float height) noexcept
    {
        return jlimit (0.1f, 10000.0f, height);
    }

    const float defaultFontHeight = 14.0f;
    String fallbackFont;
    String fallbackFontStyle;
}

typedef Typeface::Ptr (*GetTypefaceForFont) (const Font&);
GetTypefaceForFont juce_getTypefaceForFont = nullptr;

//==============================================================================
class TypefaceCache  : public DeletedAtShutdown
{
public:
    TypefaceCache()
        : counter (0)
    {
        setSize (10);
    }

    ~TypefaceCache()
    {
        clearSingletonInstance();
    }

    juce_DeclareSingleton_SingleThreaded_Minimal (TypefaceCache);

    void setSize (const int numToCache)
    {
        faces.clear();
        faces.insertMultiple (-1, CachedFace(), numToCache);
    }

    Typeface::Ptr findTypefaceFor (const Font& font)
    {
        const String faceName (font.getTypefaceName());
        const String faceStyle (font.getTypefaceStyle());

        int i;
        for (i = faces.size(); --i >= 0;)
        {
            CachedFace& face = faces.getReference(i);

            if (face.typefaceName == faceName
                 && face.typefaceStyle == faceStyle
                 && face.typeface->isSuitableForFont (font))
            {
                face.lastUsageCount = ++counter;
                return face.typeface;
            }
        }

        int replaceIndex = 0;
        size_t bestLastUsageCount = std::numeric_limits<int>::max();

        for (i = faces.size(); --i >= 0;)
        {
            const size_t lu = faces.getReference(i).lastUsageCount;

            if (bestLastUsageCount > lu)
            {
                bestLastUsageCount = lu;
                replaceIndex = i;
            }
        }

        CachedFace& face = faces.getReference (replaceIndex);
        face.typefaceName = faceName;
        face.typefaceStyle = faceStyle;
        face.lastUsageCount = ++counter;

        if (juce_getTypefaceForFont == nullptr)
            face.typeface = Font::getDefaultTypefaceForFont (font);
        else
            face.typeface = juce_getTypefaceForFont (font);

        jassert (face.typeface != nullptr); // the look and feel must return a typeface!

        if (defaultFace == nullptr && font == Font())
            defaultFace = face.typeface;

        return face.typeface;
    }

    Typeface::Ptr getDefaultTypeface() const noexcept
    {
        return defaultFace;
    }

private:
    struct CachedFace
    {
        CachedFace() noexcept
            : lastUsageCount (0)
        {
        }

        // Although it seems a bit wacky to store the name here, it's because it may be a
        // placeholder rather than a real one, e.g. "<Sans-Serif>" vs the actual typeface name.
        // Since the typeface itself doesn't know that it may have this alias, the name under
        // which it was fetched needs to be stored separately.
        String typefaceName;
        String typefaceStyle;
        size_t lastUsageCount;
        Typeface::Ptr typeface;
    };

    Array <CachedFace> faces;
    Typeface::Ptr defaultFace;
    size_t counter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TypefaceCache);
};

juce_ImplementSingleton_SingleThreaded (TypefaceCache)

void Typeface::setTypefaceCacheSize (int numFontsToCache)
{
    TypefaceCache::getInstance()->setSize (numFontsToCache);
}

//==============================================================================
class Font::SharedFontInternal  : public SingleThreadedReferenceCountedObject
{
public:
    SharedFontInternal (const String& typefaceStyle_, const float height_) noexcept
        : typefaceName (Font::getDefaultSansSerifFontName()),
          typefaceStyle (typefaceStyle_),
          height (height_),
          horizontalScale (1.0f),
          kerning (0),
          ascent (0),
          underline (false),
          typeface (typefaceStyle_ == Font::getDefaultStyle()
                        ? TypefaceCache::getInstance()->getDefaultTypeface() : nullptr)
    {
    }

    SharedFontInternal (const String& typefaceName_, const String& typefaceStyle_, const float height_) noexcept
        : typefaceName (typefaceName_),
          typefaceStyle (typefaceStyle_),
          height (height_),
          horizontalScale (1.0f),
          kerning (0),
          ascent (0),
          underline (false),
          typeface (nullptr)
    {
    }

    SharedFontInternal (const Typeface::Ptr& typeface_) noexcept
        : typefaceName (typeface_->getName()),
          typefaceStyle (typeface_->getStyle()),
          height (FontValues::defaultFontHeight),
          horizontalScale (1.0f),
          kerning (0),
          ascent (0),
          underline (false),
          typeface (typeface_)
    {
    }

    SharedFontInternal (const SharedFontInternal& other) noexcept
        : typefaceName (other.typefaceName),
          typefaceStyle (other.typefaceStyle),
          height (other.height),
          horizontalScale (other.horizontalScale),
          kerning (other.kerning),
          ascent (other.ascent),
          underline (other.underline),
          typeface (other.typeface)
    {
    }

    bool operator== (const SharedFontInternal& other) const noexcept
    {
        return height == other.height
                && underline == other.underline
                && horizontalScale == other.horizontalScale
                && kerning == other.kerning
                && typefaceName == other.typefaceName
                && typefaceStyle == other.typefaceStyle;
    }

    String typefaceName, typefaceStyle;
    float height, horizontalScale, kerning, ascent;
    bool underline;
    Typeface::Ptr typeface;
};

//==============================================================================
Font::Font()
    : font (new SharedFontInternal (Font::getDefaultStyle(), FontValues::defaultFontHeight))
{
}

Font::Font (const float fontHeight, const int styleFlags)
    : font (new SharedFontInternal (Font::getDefaultStyle(), FontValues::limitFontHeight (fontHeight)))
{
    setStyleFlags(styleFlags);
}

Font::Font (const String& typefaceName, const float fontHeight, const int styleFlags)
    : font (new SharedFontInternal (typefaceName, Font::getDefaultStyle(), FontValues::limitFontHeight (fontHeight)))
{
    setStyleFlags(styleFlags);
}

Font::Font (const String& typefaceStyle, float fontHeight)
    : font (new SharedFontInternal (typefaceStyle, FontValues::limitFontHeight (fontHeight)))
{
}

Font::Font (const String& typefaceName, const String& typefaceStyle, float fontHeight)
    : font (new SharedFontInternal (typefaceName, typefaceStyle, FontValues::limitFontHeight (fontHeight)))
{
}

Font::Font (const Typeface::Ptr& typeface)
    : font (new SharedFontInternal (typeface))
{
}

Font::Font (const Font& other) noexcept
    : font (other.font)
{
}

Font& Font::operator= (const Font& other) noexcept
{
    font = other.font;
    return *this;
}

#if JUCE_COMPILER_SUPPORTS_MOVE_SEMANTICS
Font::Font (Font&& other) noexcept
    : font (static_cast <ReferenceCountedObjectPtr <SharedFontInternal>&&> (other.font))
{
}

Font& Font::operator= (Font&& other) noexcept
{
    font = static_cast <ReferenceCountedObjectPtr <SharedFontInternal>&&> (other.font);
    return *this;
}
#endif

Font::~Font() noexcept
{
}

bool Font::operator== (const Font& other) const noexcept
{
    return font == other.font
            || *font == *other.font;
}

bool Font::operator!= (const Font& other) const noexcept
{
    return ! operator== (other);
}

void Font::dupeInternalIfShared()
{
    if (font->getReferenceCount() > 1)
        font = new SharedFontInternal (*font);
}

//==============================================================================
const String& Font::getDefaultSansSerifFontName()
{
    static const String name ("<Sans-Serif>");
    return name;
}

const String& Font::getDefaultSerifFontName()
{
    static const String name ("<Serif>");
    return name;
}

const String& Font::getDefaultMonospacedFontName()
{
    static const String name ("<Monospaced>");
    return name;
}

const String& Font::getDefaultStyle()
{
    static const String style ("<Style>");
    return style;
}

const String& Font::getTypefaceName() const noexcept
{
    return font->typefaceName;
}

void Font::setTypefaceName (const String& faceName)
{
    if (faceName != font->typefaceName)
    {
        dupeInternalIfShared();
        font->typefaceName = faceName;
        font->typeface = nullptr;
        font->ascent = 0;
    }
}

const String& Font::getTypefaceStyle() const noexcept
{
    return font->typefaceStyle;
}

void Font::setTypefaceStyle (const String& typefaceStyle)
{
    if (typefaceStyle != font->typefaceStyle)
    {
        dupeInternalIfShared();
        font->typefaceStyle = typefaceStyle;
        font->typeface = nullptr;
        font->ascent = 0;
    }
}

Typeface* Font::getTypeface() const
{
    if (font->typeface == nullptr)
        font->typeface = TypefaceCache::getInstance()->findTypefaceFor (*this);

    return font->typeface;
}

//==============================================================================
const String& Font::getFallbackFontName()
{
    return FontValues::fallbackFont;
}

void Font::setFallbackFontName (const String& name)
{
    FontValues::fallbackFont = name;

   #if JUCE_MAC || JUCE_IOS
    jassertfalse; // Note that use of a fallback font isn't currently implemented in OSX..
   #endif
}

const String& Font::getFallbackFontStyle()
{
    return FontValues::fallbackFontStyle;
}

void Font::setFallbackFontStyle (const String& style)
{
    FontValues::fallbackFontStyle = style;

   #if JUCE_MAC || JUCE_IOS
    jassertfalse; // Note that use of a fallback font isn't currently implemented in OSX..
   #endif
}

//==============================================================================
float Font::getHeight() const noexcept
{
    return font->height;
}

Font Font::withHeight (const float newHeight) const
{
    Font f (*this);
    f.setHeight (newHeight);
    return f;
}

void Font::setHeight (float newHeight)
{
    newHeight = FontValues::limitFontHeight (newHeight);

    if (font->height != newHeight)
    {
        dupeInternalIfShared();
        font->height = newHeight;
    }
}

void Font::setHeightWithoutChangingWidth (float newHeight)
{
    newHeight = FontValues::limitFontHeight (newHeight);

    if (font->height != newHeight)
    {
        dupeInternalIfShared();
        font->horizontalScale *= (font->height / newHeight);
        font->height = newHeight;
    }
}

int Font::getStyleFlags() const noexcept
{
    int styleFlags = plain;
    if (font->typefaceStyle == "Bold") styleFlags |= bold;
    if (font->typefaceStyle == "Italic") styleFlags |= italic;
    if (font->typefaceStyle == "Oblique") styleFlags |= italic;
    if (font->typefaceStyle == "Bold Italic") styleFlags |= (bold | italic);
    if (font->typefaceStyle == "Bold Oblique") styleFlags |= (bold | italic);
    if (font->underline) styleFlags |= underlined;
    return styleFlags;
}

Font Font::withStyle (const int newFlags) const
{
    Font f (*this);
    f.setStyleFlags (newFlags);
    return f;
}

void Font::setStyleFlags (const int newFlags)
{
    if (getStyleFlags() != newFlags)
    {
        dupeInternalIfShared();
        if ((newFlags & underlined) == underlined) font->underline = true;
        if (newFlags == plain || newFlags == underlined) font->typefaceStyle = "Regular";
        if (((newFlags & bold) == bold) && ((newFlags & italic) != italic)) font->typefaceStyle = "Bold";
        if (((newFlags & bold) != bold) && ((newFlags & italic) == italic)) font->typefaceStyle = "Italic";
        if ((newFlags & (bold | italic)) == (bold | italic)) font->typefaceStyle = "Bold Italic";
        font->typeface = nullptr;
        font->ascent = 0;
    }
}

void Font::setSizeAndStyle (float newHeight,
                            const int newStyleFlags,
                            const float newHorizontalScale,
                            const float newKerningAmount)
{
    newHeight = FontValues::limitFontHeight (newHeight);

    if (font->height != newHeight
         || font->horizontalScale != newHorizontalScale
         || font->kerning != newKerningAmount)
    {
        dupeInternalIfShared();
        font->height = newHeight;
        font->horizontalScale = newHorizontalScale;
        font->kerning = newKerningAmount;
    }

    setStyleFlags (newStyleFlags);
}

void Font::setSizeAndStyle (float newHeight,
                            const String& newStyle,
                            const float newHorizontalScale,
                            const float newKerningAmount)
{
    newHeight = FontValues::limitFontHeight (newHeight);

    if (font->height != newHeight
         || font->horizontalScale != newHorizontalScale
         || font->kerning != newKerningAmount)
    {
        dupeInternalIfShared();
        font->height = newHeight;
        font->horizontalScale = newHorizontalScale;
        font->kerning = newKerningAmount;
    }

    setTypefaceStyle (newStyle);
}

float Font::getHorizontalScale() const noexcept
{
    return font->horizontalScale;
}

Font Font::withHorizontalScale (const float newHorizontalScale) const
{
    Font f (*this);
    f.setHorizontalScale (newHorizontalScale);
    return f;
}

void Font::setHorizontalScale (const float scaleFactor)
{
    dupeInternalIfShared();
    font->horizontalScale = scaleFactor;
}

float Font::getExtraKerningFactor() const noexcept
{
    return font->kerning;
}

Font Font::withExtraKerningFactor (const float extraKerning) const
{
    Font f (*this);
    f.setExtraKerningFactor (extraKerning);
    return f;
}

void Font::setExtraKerningFactor (const float extraKerning)
{
    dupeInternalIfShared();
    font->kerning = extraKerning;
}

Font Font::boldened() const             { return withStyle (getStyleFlags() | bold); }
Font Font::italicised() const           { return withStyle (getStyleFlags() | italic); }

bool Font::isBold() const noexcept
{
    if (font->typefaceStyle == "Bold") return true;
    if (font->typefaceStyle == "Bold Italic") return true;
    if (font->typefaceStyle == "Bold Oblique") return true;
    return false;
}

bool Font::isItalic() const noexcept
{
    if (font->typefaceStyle == "Italic") return true;
    if (font->typefaceStyle == "Oblique") return true;
    if (font->typefaceStyle == "Bold Italic") return true;
    if (font->typefaceStyle == "Bold Oblique") return true;
    return false;
}

void Font::setBold (const bool shouldBeBold)
{
    setStyleFlags (shouldBeBold ? (getStyleFlags() | bold)
                                : (getStyleFlags() & ~bold));
}

void Font::setItalic (const bool shouldBeItalic)
{
    setStyleFlags (shouldBeItalic ? (getStyleFlags() | italic)
                                  : (getStyleFlags() & ~italic));
}

void Font::setUnderline (const bool shouldBeUnderlined)
{
    font->underline = shouldBeUnderlined;
}

bool Font::isUnderlined() const noexcept
{
    return font->underline;
}

float Font::getAscent() const
{
    if (font->ascent == 0)
        font->ascent = getTypeface()->getAscent();

    return font->height * font->ascent;
}

float Font::getDescent() const
{
    return font->height - getAscent();
}

int Font::getStringWidth (const String& text) const
{
    return roundToInt (getStringWidthFloat (text));
}

float Font::getStringWidthFloat (const String& text) const
{
    float w = getTypeface()->getStringWidth (text);

    if (font->kerning != 0)
        w += font->kerning * text.length();

    return w * font->height * font->horizontalScale;
}

void Font::getGlyphPositions (const String& text, Array <int>& glyphs, Array <float>& xOffsets) const
{
    getTypeface()->getGlyphPositions (text, glyphs, xOffsets);

    const int num = xOffsets.size();

    if (num > 0)
    {
        const float scale = font->height * font->horizontalScale;
        float* const x = xOffsets.getRawDataPointer();

        if (font->kerning != 0)
        {
            for (int i = 0; i < num; ++i)
                x[i] = (x[i] + i * font->kerning) * scale;
        }
        else
        {
            for (int i = 0; i < num; ++i)
                x[i] *= scale;
        }
    }
}

void Font::findFonts (Array<Font>& destArray)
{
    const StringArray names (findAllTypefaceNames());

    for (int i = 0; i < names.size(); ++i)
    {
        const StringArray styles (findAllTypefaceStyles (names[i]));
        for (int j = 0; j < styles.size(); ++j)
            destArray.add (Font (names[i],  styles[j], FontValues::defaultFontHeight));
    }
}

//==============================================================================
String Font::toString() const
{
    String s (getTypefaceName());
    s += "; " + getTypefaceStyle() + "; ";
    s += String (getHeight(), 1);

    return s;
}

Font Font::fromString (const String& fontDescription)
{
    int start = 0;
    int separator = fontDescription.indexOfChar (';');

    String name = fontDescription.substring (start, separator).trim();
    start = separator + 1;
    separator = fontDescription.indexOfChar (start, ';');
    String style = fontDescription.substring (start, separator).trim();
    String sizeAndStyle (fontDescription.substring (separator + 1));

    float height = sizeAndStyle.getFloatValue();
    if (height <= 0)
        height = 10.0f;

    return Font (name, style, height);
}
