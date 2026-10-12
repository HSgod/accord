/*
 * Accord - Tests for the audio port variants the console session compares.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tests/micro_test.hpp"

#include <string>
#include <string_view>

#include "daemon/src/variant.hpp"

using namespace accord::daemon;

MICRO_TEST(variant_zero_is_the_configuration_this_console_accepted)
{
    // The system user, the type the kit opens, and the volume flags the console
    // answered 0 to. The first console run had none of the three right.
    const AudioVariant &first = audio_variant_at(0);
    MICRO_CHECK_EQ(first.user, kUserSystem);
    MICRO_CHECK_EQ(first.type, 0);
    MICRO_CHECK_EQ(first.volume_flags, 3);
    MICRO_CHECK(!first.name.empty());
}

MICRO_TEST(variant_grid_holds_both_user_ids_and_the_volume_flags_to_try)
{
    MICRO_CHECK_EQ(kAudioVariantCount, std::size_t{6});

    bool system_seen = false;
    bool account_seen = false;
    bool volume_flags_three_seen = false;
    bool volume_flags_zero_seen = false;
    bool no_volume_call_seen = false;

    for (std::size_t index = 0; index < kAudioVariantCount; ++index)
    {
        const AudioVariant &variant = audio_variant_at(index);
        if (variant.user == kUserSystem)
            system_seen = true;
        if (variant.user == kUserAccount)
            account_seen = true;
        if (variant.volume_flags == 3)
            volume_flags_three_seen = true;
        if (variant.volume_flags == 0)
            volume_flags_zero_seen = true;
        if (variant.volume_flags == kNoVolumeCall)
            no_volume_call_seen = true;
    }

    MICRO_CHECK(system_seen);
    MICRO_CHECK(account_seen);
    MICRO_CHECK(volume_flags_three_seen);
    MICRO_CHECK(volume_flags_zero_seen); // the flags the console answered 0x80260013 to
    MICRO_CHECK(no_volume_call_seen);    // the kit's path, kept as a control
}

MICRO_TEST(variant_grid_keeps_the_type_the_console_opened)
{
    // Only the types 0 and 1 opened on this console; the chat type 2 came back
    // 0x80260011 on every user id it was tried with, so no variant carries it.
    for (std::size_t index = 0; index < kAudioVariantCount; ++index)
    {
        const int type = audio_variant_at(index).type;
        MICRO_CHECK(type == 0 || type == 1);
    }
}

MICRO_TEST(variant_names_are_distinct_and_free_of_quotes)
{
    for (std::size_t index = 0; index < kAudioVariantCount; ++index)
    {
        const std::string_view name = audio_variant_at(index).name;
        MICRO_CHECK(!name.empty());
        MICRO_CHECK(name.find('"') == std::string_view::npos);
        MICRO_CHECK(name.find('\\') == std::string_view::npos);

        for (std::size_t other = index + 1; other < kAudioVariantCount; ++other)
            MICRO_CHECK(name != audio_variant_at(other).name);
    }
}

MICRO_TEST(variant_index_past_the_grid_answers_with_the_first_one)
{
    MICRO_CHECK_EQ(audio_variant_at(kAudioVariantCount).type, audio_variant_at(0).type);
    MICRO_CHECK_EQ(std::string{audio_variant_at(99).name}, std::string{audio_variant_at(0).name});
}

MICRO_TEST(variant_text_parses_only_a_number_inside_the_grid)
{
    bool ok = false;

    MICRO_CHECK_EQ(audio_variant_from_text("2", ok), std::size_t{2});
    MICRO_CHECK(ok);

    MICRO_CHECK_EQ(audio_variant_from_text("0", ok), std::size_t{0});
    MICRO_CHECK(ok);

    audio_variant_from_text("6", ok);
    MICRO_CHECK(!ok); // one past the end
    audio_variant_from_text("", ok);
    MICRO_CHECK(!ok);
    audio_variant_from_text("x", ok);
    MICRO_CHECK(!ok);
    audio_variant_from_text("2x", ok);
    MICRO_CHECK(!ok);
    audio_variant_from_text("-1", ok);
    MICRO_CHECK(!ok);
    audio_variant_from_text("100", ok);
    MICRO_CHECK(!ok);
}

MICRO_TEST(variant_name_knows_main_and_refuses_voice)
{
    bool known = false;

    MICRO_CHECK_EQ(audio_variant_from_name("main", known), std::size_t{0});
    MICRO_CHECK(known);
    MICRO_CHECK_EQ(audio_variant_at(0).type, 0);

    // --audio=voice has nothing to select while the console refuses the chat
    // type, and a name the daemon cannot honour is better reported than obeyed.
    audio_variant_from_name("voice", known);
    MICRO_CHECK(!known);

    audio_variant_from_name("nope", known);
    MICRO_CHECK(!known);
}
