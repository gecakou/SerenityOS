/*
 * Copyright (c) 2022, Nico Weber <thakis@chromium.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <LibGfx/ICCProfile.h>
#include <math.h>
#include <time.h>

// V2 spec: https://color.org/specification/ICC.1-2001-04.pdf
// V4 spec: https://color.org/specification/ICC.1-2022-05.pdf

namespace Gfx::ICC {

namespace {

// ICC V4, 4.2 dateTimeNumber
// "All the dateTimeNumber values in a profile shall be in Coordinated Universal Time [...]."
struct DateTimeNumber {
    BigEndian<u16> year;
    BigEndian<u16> month;
    BigEndian<u16> day;
    BigEndian<u16> hours;
    BigEndian<u16> minutes;
    BigEndian<u16> seconds;
};

// ICC V4, 4.6 s15Fixed16Number
using s15Fixed16Number = i32;

// ICC V4, 4.14 XYZNumber
struct XYZNumber {
    BigEndian<s15Fixed16Number> x;
    BigEndian<s15Fixed16Number> y;
    BigEndian<s15Fixed16Number> z;

    operator XYZ() const
    {
        return XYZ { x / (double)0x1'0000, y / (double)0x1'0000, z / (double)0x1'0000 };
    }
};

ErrorOr<time_t> parse_date_time_number(DateTimeNumber const& date_time)
{
    // ICC V4, 4.2 dateTimeNumber

    // "Number of the month (1 to 12)"
    if (date_time.month < 1 || date_time.month > 12)
        return Error::from_string_literal("ICC::Profile: dateTimeNumber month out of bounds");

    // "Number of the day of the month (1 to 31)"
    if (date_time.day < 1 || date_time.day > 31)
        return Error::from_string_literal("ICC::Profile: dateTimeNumber day out of bounds");

    // "Number of hours (0 to 23)"
    if (date_time.hours > 23)
        return Error::from_string_literal("ICC::Profile: dateTimeNumber hours out of bounds");

    // "Number of minutes (0 to 59)"
    if (date_time.minutes > 59)
        return Error::from_string_literal("ICC::Profile: dateTimeNumber minutes out of bounds");

    // "Number of seconds (0 to 59)"
    // ICC profiles apparently can't be created during leap seconds (seconds would be 60 there, but the spec doesn't allow that).
    if (date_time.seconds > 59)
        return Error::from_string_literal("ICC::Profile: dateTimeNumber seconds out of bounds");

    struct tm tm = {};
    tm.tm_year = date_time.year - 1900;
    tm.tm_mon = date_time.month - 1;
    tm.tm_mday = date_time.day;
    tm.tm_hour = date_time.hours;
    tm.tm_min = date_time.minutes;
    tm.tm_sec = date_time.seconds;
    // timegm() doesn't read tm.tm_isdst, tm.tm_wday, and tm.tm_yday, no need to fill them in.

    time_t timestamp = timegm(&tm);
    if (timestamp == -1)
        return Error::from_string_literal("ICC::Profile: dateTimeNumber not representable as timestamp");

    return timestamp;
}

// ICC V4, 7.2 Profile header
struct ICCHeader {
    BigEndian<u32> profile_size;
    BigEndian<u32> preferred_cmm_type;

    u8 profile_version_major;
    u8 profile_version_minor_bugfix;
    BigEndian<u16> profile_version_zero;

    BigEndian<DeviceClass> profile_device_class;
    BigEndian<ColorSpace> data_color_space;
    BigEndian<ColorSpace> profile_connection_space; // "PCS" in the spec.

    DateTimeNumber profile_creation_time;

    BigEndian<u32> profile_file_signature;
    BigEndian<PrimaryPlatform> primary_platform;

    BigEndian<u32> profile_flags;
    BigEndian<u32> device_manufacturer;
    BigEndian<u32> device_model;
    BigEndian<u64> device_attributes;
    BigEndian<u32> rendering_intent;

    XYZNumber pcs_illuminant;

    BigEndian<u32> profile_creator;

    u8 profile_id[16];
    u8 reserved[28];
};
static_assert(sizeof(ICCHeader) == 128);

Optional<PreferredCMMType> parse_preferred_cmm_type(ICCHeader const& header)
{
    // ICC v4, 7.2.3 Preferred CMM type field

    // "This field may be used to identify the preferred CMM to be used.
    //  If used, it shall match a CMM type signature registered in the ICC Tag Registry"
    // https://www.color.org/signatures2.xalter currently links to
    // https://www.color.org/registry/signature/TagRegistry-2021-03.pdf, which contains
    // some CMM signatures.
    // This requirement is often honored in practice, but not always. For example,
    // JPEGs exported in Adobe Lightroom contain profiles that set this to 'Lino',
    // which is not present in the "CMM Signatures" table in that PDF.

    // "If no preferred CMM is identified, this field shall be set to zero (00000000h)."
    if (header.preferred_cmm_type == 0)
        return {};
    return PreferredCMMType { header.preferred_cmm_type };
}

ErrorOr<Version> parse_version(ICCHeader const& header)
{
    // ICC v4, 7.2.4 Profile version field
    if (header.profile_version_zero != 0)
        return Error::from_string_literal("ICC::Profile: Reserved version bytes not zero");
    return Version(header.profile_version_major, header.profile_version_minor_bugfix);
}

ErrorOr<DeviceClass> parse_device_class(ICCHeader const& header)
{
    // ICC v4, 7.2.5 Profile/device class field
    switch (header.profile_device_class) {
    case DeviceClass::InputDevce:
    case DeviceClass::DisplayDevice:
    case DeviceClass::OutputDevice:
    case DeviceClass::DeviceLink:
    case DeviceClass::ColorSpace:
    case DeviceClass::Abstract:
    case DeviceClass::NamedColor:
        return header.profile_device_class;
    }
    return Error::from_string_literal("ICC::Profile: Invalid device class");
}

ErrorOr<ColorSpace> parse_color_space(ColorSpace color_space)
{
    // ICC v4, Table 19 — Data colour space signatures
    switch (color_space) {
    case ColorSpace::nCIEXYZ:
    case ColorSpace::CIELAB:
    case ColorSpace::CIELUV:
    case ColorSpace::YCbCr:
    case ColorSpace::CIEYxy:
    case ColorSpace::RGB:
    case ColorSpace::Gray:
    case ColorSpace::HSV:
    case ColorSpace::HLS:
    case ColorSpace::CMYK:
    case ColorSpace::CMY:
    case ColorSpace::TwoColor:
    case ColorSpace::ThreeColor:
    case ColorSpace::FourColor:
    case ColorSpace::FiveColor:
    case ColorSpace::SixColor:
    case ColorSpace::SevenColor:
    case ColorSpace::EightColor:
    case ColorSpace::NineColor:
    case ColorSpace::TenColor:
    case ColorSpace::ElevenColor:
    case ColorSpace::TwelveColor:
    case ColorSpace::ThirteenColor:
    case ColorSpace::FourteenColor:
    case ColorSpace::FifteenColor:
        return color_space;
    }
    return Error::from_string_literal("ICC::Profile: Invalid color space");
}

ErrorOr<ColorSpace> parse_data_color_space(ICCHeader const& header)
{
    // ICC v4, 7.2.6 Data colour space field
    return parse_color_space(header.data_color_space);
}

ErrorOr<ColorSpace> parse_connection_space(ICCHeader const& header)
{
    // ICC v4, 7.2.7 PCS field
    //         and Annex D
    auto space = TRY(parse_color_space(header.profile_connection_space));

    if (header.profile_device_class != DeviceClass::DeviceLink && (space != ColorSpace::PCSXYZ && space != ColorSpace::PCSLAB))
        return Error::from_string_literal("ICC::Profile: Invalid profile connection space: Non-PCS space on non-DeviceLink profile");

    return space;
}

ErrorOr<time_t> parse_creation_date_time(ICCHeader const& header)
{
    // ICC v4, 7.2.8 Date and time field
    return parse_date_time_number(header.profile_creation_time);
}

ErrorOr<void> parse_file_signature(ICCHeader const& header)
{
    // ICC v4, 7.2.9 Profile file signature field
    // "The profile file signature field shall contain the value “acsp” (61637370h) as a profile file signature."
    if (header.profile_file_signature != 0x61637370)
        return Error::from_string_literal("ICC::Profile: profile file signature not 'acsp'");
    return {};
}

ErrorOr<PrimaryPlatform> parse_primary_platform(ICCHeader const& header)
{
    // ICC v4, 7.2.10 Primary platform field
    switch (header.primary_platform) {
    case PrimaryPlatform::Apple:
    case PrimaryPlatform::Microsoft:
    case PrimaryPlatform::SiliconGraphics:
    case PrimaryPlatform::Sun:
        return header.primary_platform;
    }
    return Error::from_string_literal("ICC::Profile: Invalid primary platform");
}

Optional<DeviceManufacturer> parse_device_manufacturer(ICCHeader const& header)
{
    // ICC v4, 7.2.12 Device manufacturer field
    // "This field may be used to identify a device manufacturer.
    //  If used the signature shall match the signature contained in the appropriate section of the ICC signature registry found at www.color.org"
    // Device manufacturers can be looked up at https://www.color.org/signatureRegistry/index.xalter
    // For example: https://www.color.org/signatureRegistry/?entityEntry=APPL-4150504C
    // Some icc files use codes not in that registry. For example. D50_XYZ.icc from https://www.color.org/XYZprofiles.xalter
    // has its device manufacturer set to 'none', but https://www.color.org/signatureRegistry/?entityEntry=none-6E6F6E65 does not exist.

    // "If not used this field shall be set to zero (00000000h)."
    if (header.device_manufacturer == 0)
        return {};
    return DeviceManufacturer { header.device_manufacturer };
}

Optional<DeviceModel> parse_device_model(ICCHeader const& header)
{
    // ICC v4, 7.2.13 Device model field
    // "This field may be used to identify a device model.
    //  If used the signature shall match the signature contained in the appropriate section of the ICC signature registry found at www.color.org"
    // Device models can be looked up at https://www.color.org/signatureRegistry/deviceRegistry/index.xalter
    // For example: https://www.color.org/signatureRegistry/deviceRegistry/?entityEntry=7FD8-37464438
    // Some icc files use codes not in that registry. For example. D50_XYZ.icc from https://www.color.org/XYZprofiles.xalter
    // has its device model set to 'none', but https://www.color.org/signatureRegistry/deviceRegistry?entityEntry=none-6E6F6E65 does not exist.

    // "If not used this field shall be set to zero (00000000h)."
    if (header.device_model == 0)
        return {};
    return DeviceModel { header.device_model };
}

ErrorOr<DeviceAttributes> parse_device_attributes(ICCHeader const& header)
{
    // ICC v4, 7.2.14 Device attributes field

    // "4 to 31": "Reserved (set to binary zero)"
    if (header.device_attributes & 0xffff'fff0)
        return Error::from_string_literal("ICC::Profile: Device attributes reserved bits not set to 0");

    return DeviceAttributes { header.device_attributes };
}

ErrorOr<RenderingIntent> parse_rendering_intent(ICCHeader const& header)
{
    // ICC v4, 7.2.15 Rendering intent field
    switch (header.rendering_intent) {
    case 0:
        return RenderingIntent::Perceptual;
    case 1:
        return RenderingIntent::MediaRelativeColorimetric;
    case 2:
        return RenderingIntent::Saturation;
    case 3:
        return RenderingIntent::ICCAbsoluteColorimetric;
    }
    return Error::from_string_literal("ICC::Profile: Invalid rendering intent");
}

ErrorOr<XYZ> parse_pcs_illuminant(ICCHeader const& header)
{
    // ICC v4, 7.2.16 PCS illuminant field
    XYZ xyz = (XYZ)header.pcs_illuminant;

    /// "The value, when rounded to four decimals, shall be X = 0,9642, Y = 1,0 and Z = 0,8249."
    if (round(xyz.x * 10'000) != 9'642 || round(xyz.y * 10'000) != 10'000 || round(xyz.z * 10'000) != 8'249)
        return Error::from_string_literal("ICC::Profile: Invalid pcs illuminant");

    return xyz;
}

Optional<Creator> parse_profile_creator(ICCHeader const& header)
{
    // ICC v4, 7.2.17 Profile creator field
    // "This field may be used to identify the creator of the profile.
    //  If used the signature should match the signature contained in the device manufacturer section of the ICC signature registry found at www.color.org."
    // This is not always true in practice.
    // For example, .icc files in /System/ColorSync/Profiles on macOS 12.6 set this to 'appl', which is a CMM signature, not a device signature (that one would be 'APPL').

    // "If not used this field shall be set to zero (00000000h)."
    if (header.profile_creator == 0)
        return {};
    return Creator { header.profile_creator };
}

template<size_t N>
bool all_bytes_are_zero(const u8 (&bytes)[N])
{
    for (u8 byte : bytes) {
        if (byte != 0)
            return false;
    }
    return true;
}

ErrorOr<Optional<Crypto::Hash::MD5::DigestType>> parse_profile_id(ICCHeader const& header, ReadonlyBytes icc_bytes)
{
    // ICC v4, 7.2.18 Profile ID field
    // "A profile ID field value of zero (00h) shall indicate that a profile ID has not been calculated."
    if (all_bytes_are_zero(header.profile_id))
        return Optional<Crypto::Hash::MD5::DigestType> {};

    Crypto::Hash::MD5::DigestType id;
    static_assert(sizeof(id.data) == sizeof(header.profile_id));
    memcpy(id.data, header.profile_id, sizeof(id.data));

    auto computed_id = Profile::compute_id(icc_bytes);
    if (id != computed_id)
        return Error::from_string_literal("ICC::Profile: Invalid profile id");

    return id;
}

ErrorOr<void> parse_reserved(ICCHeader const& header)
{
    // ICC v4, 7.2.19 Reserved field
    // "This field of the profile header is reserved for future ICC definition and shall be set to zero."
    if (!all_bytes_are_zero(header.reserved))
        return Error::from_string_literal("ICC::Profile: Reserved header bytes are not zero");
    return {};
}
}

StringView device_class_name(DeviceClass device_class)
{
    switch (device_class) {
    case DeviceClass::InputDevce:
        return "InputDevce"sv;
    case DeviceClass::DisplayDevice:
        return "DisplayDevice"sv;
    case DeviceClass::OutputDevice:
        return "OutputDevice"sv;
    case DeviceClass::DeviceLink:
        return "DeviceLink"sv;
    case DeviceClass::ColorSpace:
        return "ColorSpace"sv;
    case DeviceClass::Abstract:
        return "Abstract"sv;
    case DeviceClass::NamedColor:
        return "NamedColor"sv;
    }
    VERIFY_NOT_REACHED();
}

StringView data_color_space_name(ColorSpace color_space)
{
    switch (color_space) {
    case ColorSpace::nCIEXYZ:
        return "nCIEXYZ"sv;
    case ColorSpace::CIELAB:
        return "CIELAB"sv;
    case ColorSpace::CIELUV:
        return "CIELUV"sv;
    case ColorSpace::YCbCr:
        return "YCbCr"sv;
    case ColorSpace::CIEYxy:
        return "CIEYxy"sv;
    case ColorSpace::RGB:
        return "RGB"sv;
    case ColorSpace::Gray:
        return "Gray"sv;
    case ColorSpace::HSV:
        return "HSV"sv;
    case ColorSpace::HLS:
        return "HLS"sv;
    case ColorSpace::CMYK:
        return "CMYK"sv;
    case ColorSpace::CMY:
        return "CMY"sv;
    case ColorSpace::TwoColor:
        return "2 color"sv;
    case ColorSpace::ThreeColor:
        return "3 color (other than XYZ, Lab, Luv, YCbCr, CIEYxy, RGB, HSV, HLS, CMY)"sv;
    case ColorSpace::FourColor:
        return "4 color (other than CMYK)"sv;
    case ColorSpace::FiveColor:
        return "5 color"sv;
    case ColorSpace::SixColor:
        return "6 color"sv;
    case ColorSpace::SevenColor:
        return "7 color"sv;
    case ColorSpace::EightColor:
        return "8 color"sv;
    case ColorSpace::NineColor:
        return "9 color"sv;
    case ColorSpace::TenColor:
        return "10 color"sv;
    case ColorSpace::ElevenColor:
        return "11 color"sv;
    case ColorSpace::TwelveColor:
        return "12 color"sv;
    case ColorSpace::ThirteenColor:
        return "13 color"sv;
    case ColorSpace::FourteenColor:
        return "14 color"sv;
    case ColorSpace::FifteenColor:
        return "15 color"sv;
    }
    VERIFY_NOT_REACHED();
}

StringView profile_connection_space_name(ColorSpace color_space)
{
    switch (color_space) {
    case ColorSpace::PCSXYZ:
        return "PCSXYZ"sv;
    case ColorSpace::PCSLAB:
        return "PCSLAB"sv;
    default:
        return data_color_space_name(color_space);
    }
}

StringView primary_platform_name(PrimaryPlatform primary_platform)
{
    switch (primary_platform) {
    case PrimaryPlatform::Apple:
        return "Apple"sv;
    case PrimaryPlatform::Microsoft:
        return "Microsoft"sv;
    case PrimaryPlatform::SiliconGraphics:
        return "Silicon Graphics"sv;
    case PrimaryPlatform::Sun:
        return "Sun"sv;
    }
    VERIFY_NOT_REACHED();
}

StringView rendering_intent_name(RenderingIntent rendering_intent)
{
    switch (rendering_intent) {
    case RenderingIntent::Perceptual:
        return "Perceptual"sv;
    case RenderingIntent::MediaRelativeColorimetric:
        return "Media-relative colorimetric"sv;
    case RenderingIntent::Saturation:
        return "Saturation"sv;
    case RenderingIntent::ICCAbsoluteColorimetric:
        return "ICC-absolute colorimetric"sv;
    }
    VERIFY_NOT_REACHED();
}

Flags::Flags() = default;
Flags::Flags(u32 bits)
    : m_bits(bits)
{
}

DeviceAttributes::DeviceAttributes() = default;
DeviceAttributes::DeviceAttributes(u64 bits)
    : m_bits(bits)
{
}

ErrorOr<NonnullRefPtr<Profile>> Profile::try_load_from_externally_owned_memory(ReadonlyBytes bytes)
{
    auto profile = adopt_ref(*new Profile());

    if (bytes.size() < sizeof(ICCHeader))
        return Error::from_string_literal("ICC::Profile: Not enough data for header");

    auto header = *bit_cast<ICCHeader const*>(bytes.data());

    TRY(parse_file_signature(header));
    profile->m_preferred_cmm_type = parse_preferred_cmm_type(header);
    profile->m_version = TRY(parse_version(header));
    profile->m_device_class = TRY(parse_device_class(header));
    profile->m_data_color_space = TRY(parse_data_color_space(header));
    profile->m_connection_space = TRY(parse_connection_space(header));
    profile->m_creation_timestamp = TRY(parse_creation_date_time(header));
    profile->m_primary_platform = TRY(parse_primary_platform(header));
    profile->m_flags = Flags { header.profile_flags };
    profile->m_device_manufacturer = parse_device_manufacturer(header);
    profile->m_device_model = parse_device_model(header);
    profile->m_device_attributes = TRY(parse_device_attributes(header));
    profile->m_rendering_intent = TRY(parse_rendering_intent(header));
    profile->m_pcs_illuminant = TRY(parse_pcs_illuminant(header));
    profile->m_creator = parse_profile_creator(header);
    profile->m_id = TRY(parse_profile_id(header, bytes));
    TRY(parse_reserved(header));

    return profile;
}

Crypto::Hash::MD5::DigestType Profile::compute_id(ReadonlyBytes bytes)
{
    // ICC v4, 7.2.18 Profile ID field
    // "The Profile ID shall be calculated using the MD5 fingerprinting method as defined in Internet RFC 1321.
    //  The entire profile, whose length is given by the size field in the header, with the
    //  profile flags field (bytes 44 to 47, see 7.2.11),
    //  rendering intent field (bytes 64 to 67, see 7.2.15),
    //  and profile ID field (bytes 84 to 99)
    //  in the profile header temporarily set to zeros (00h),
    //  shall be used to calculate the ID."
    const u8 zero[16] = {};
    Crypto::Hash::MD5 md5;
    md5.update(bytes.slice(0, 44));
    md5.update(ReadonlyBytes { zero, 4 }); // profile flags field
    md5.update(bytes.slice(48, 64 - 48));
    md5.update(ReadonlyBytes { zero, 4 }); // rendering intent field
    md5.update(bytes.slice(68, 84 - 68));
    md5.update(ReadonlyBytes { zero, 16 }); // profile ID field
    md5.update(bytes.slice(100));
    return md5.digest();
}

}
