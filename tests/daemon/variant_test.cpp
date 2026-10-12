/*
 * Accord - Tests for the audio port variants the console session compares.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tests/micro_test.hpp"

#include <string>
#include <string_view>

#include "daemon/src/variant.hpp"

using namespace accord::daemon;

MICRO_TEST(variant_grid_pairs_a_type_with_and_without_the_volume_call)
{
    MICRO_CHECK_EQ(kAudioVariantCount, std::size_t{6});

    // Neighbours differ in the volume call alone, so a listener hears the same
    // type twice in a row and the only new variable is the call itself.
    for (std::size_t index = 0; index + 1 < kAudioVariantCount; index += 2)
    {
        MICRO_CHECK_EQ(audio_variant_at(index).type, audio_variant_at(index + 1).type);
        MICRO_CHECK(audio_variant_at(index).set_volume);
        MICRO_CHECK(!audio_variant_at(index + 1).set_volume);
    }
}

MICRO_TEST(variant_zero_is_the_port_the_kit_opens)
{
    const AudioVariant &first = audio_variant_at(0);
    MICRO_CHECK_EQ(first.type, 0);
    MICRO_CHECK(first.set_volume);
    MICRO_CHECK(!first.name.empty());
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

MICRO_TEST(variant_name_selects_the_type_the_command_line_asked_for)
{
    bool known = false;

    MICRO_CHECK_EQ(audio_variant_from_name("main", known), std::size_t{0});
    MICRO_CHECK(known);
    MICRO_CHECK_EQ(audio_variant_at(0).type, 0);

    const std::size_t chat = audio_variant_from_name("voice", known);
    MICRO_CHECK(known);
    MICRO_CHECK_EQ(audio_variant_at(chat).type, 2);

    audio_variant_from_name("nope", known);
    MICRO_CHECK(!known);
}
