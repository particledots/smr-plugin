// Derived from the Spectral Multiband Resonator firmware (third_party/SMR, v5.1.1, commit 79bc953).
// Original authors: Dan Green, Hugo Paris. Copyright (c) 2015 Dan Green. MIT licence: see THIRD-PARTY-LICENSES.txt and licenses/SMR-LICENSE.txt.
// Port of third_party/SMR/rotation.c. Statement order and integer types follow the C source.
// The only additions are the search-loop guards (docs/port-notes.md item P-7): every unbounded
// `while (!is_distinct)` in the C gets a counter that gives up after a full cycle of the 20 filter
// positions. The firmware would hang there; no state that the firmware can leave is changed.
#include "Engine.h"

namespace smr
{
    namespace
    {
        // The loops step one position at a time round a ring of kNumFilts positions and test conditions that
        // do not change while the loop runs, so no success is possible after kNumFilts further steps.
        constexpr int kMaxSearchSteps = kNumFilts;
    }

    // rotation.c change_scale_up
    void Engine::change_scale_up()
    {
        uint8_t i;

        for (i = 0; i < kNumChannels; i++)
        {
            if (lock[i] != 1)
            {
                if (motion_scale_dest[i] < (kNumScales - 1))
                    motion_scale_dest[i]++;
            }
        }
    }

    // rotation.c change_scale_down
    void Engine::change_scale_down()
    {
        uint8_t i;

        for (i = 0; i < kNumChannels; i++)
        {
            if (lock[i] != 1)
            {
                if (motion_scale_dest[i] > 0)
                {
                    motion_scale_dest[i]--;
                }
            }
        }
    }

    // rotation.c jump_rotate_with_cv
    void Engine::jump_rotate_with_cv (int8_t shift_amt)
    {
        motion_notejump = (int8_t) (motion_notejump + shift_amt);
    }

    // rotation.c jump_scale_with_cv
    void Engine::jump_scale_with_cv (int8_t shift_amt)
    {
        uint8_t i;
        int8_t this_shift_amt;

        for (i = 0; i < kNumChannels; i++)
        {
            if (lock[i] != 1)
            {
                this_shift_amt = shift_amt;

                if (this_shift_amt < 0 && motion_scalecv_overage[i] > 0)
                {
                    if ((-1 * this_shift_amt) > motion_scalecv_overage[i])
                    {
                        this_shift_amt = (int8_t) (this_shift_amt + motion_scalecv_overage[i]);
                        motion_scalecv_overage[i] = 0;
                    }
                    else
                    {
                        motion_scalecv_overage[i] = (int8_t) (motion_scalecv_overage[i] + this_shift_amt);
                        this_shift_amt = 0;
                    }
                }

                motion_scale_dest[i] = (int8_t) (motion_scale_dest[i] + this_shift_amt);

                if (motion_scale_dest[i] < 0)
                {
                    motion_scale_dest[i] = 0;
                }
                if (motion_scale_dest[i] > (kNumScales - 1))
                {
                    motion_scalecv_overage[i] = (int8_t) (motion_scale_dest[i] - (kNumScales - 1));
                    motion_scale_dest[i] = kNumScales - 1;
                }
            }
        }
    }

    // rotation.c rotate_down
    void Engine::rotate_down()
    {
        //if we were rotating down, reverse direction. Otherwise add to current direction
        if (motion_rotate >= 0) motion_rotate = -1;
        else motion_rotate = (int8_t) (motion_rotate - 1);
    }

    // rotation.c rotate_up
    void Engine::rotate_up()
    {
        if (motion_rotate <= 0) motion_rotate = 1;
        else motion_rotate = (int8_t) (motion_rotate + 1);
    }

    // rotation.c is_morphing
    bool Engine::is_morphing() const
    {
        if (motion_morphpos[0] == 0.0f
            && motion_morphpos[1] == 0.0f
            && motion_morphpos[2] == 0.0f
            && motion_morphpos[3] == 0.0f
            && motion_morphpos[4] == 0.0f
            && motion_morphpos[5] == 0.0f)
            return false;
        else return true;
    }

    // rotation.c update_spread
    void Engine::update_spread (int8_t t_spread)
    {
        float spread_out;
        int32_t i, test_spot, offset, base_note;
        uint8_t j;
        uint8_t k;
        uint8_t is_distinct;

        int32_t test_motion[6] = { 0, 0, 0, 0, 0, 0 };

        base_note = motion_fadeto_note[2];

        spread = t_spread;

        if (spread > old_spread) spread_out = 1.0f;
        else spread_out = -1.0f;

        old_spread = spread;

        test_motion[0] = 99;
        test_motion[1] = 99;
        test_motion[3] = 99;
        test_motion[4] = 99;
        test_motion[5] = 99;

        base_note = motion_fadeto_note[2];

        for (i = 0; i < kNumChannels; i++)
        {
            if (lock[i] == 1 || i == 2)
            {
                test_motion[i] = motion_fadeto_note[i];
            }
            else
            {
                //Set spread direction. Note spread_dir[2] remains 0 (stationary)
                if (i < 2) motion_spread_dir[i] = (int8_t) (-1.0f * spread_out);
                if (i > 2) motion_spread_dir[i] = (int8_t) spread_out;

                //Find an open filter channel:
                //Our starting point is based on the location of channel 2, and spread outward from there
                offset = (((int32_t) i) - 2) * spread;
                test_spot = base_note + offset;

                //Set up test_spot, since the first thing we do is in the loop is test_spot += spread_dir[i]
                test_spot -= motion_spread_dir[i];

                //Now check to see if test_spot is open
                is_distinct = 0;
                k = 0;
                while ((! is_distinct) && (k < kNumFilts + 1))
                {
                    test_spot += motion_spread_dir[i];

                    while (test_spot >= kNumFilts)
                    {
                        test_spot -= kNumFilts;
                    }
                    while (test_spot < 0)
                    {
                        test_spot += kNumFilts;
                    }

                    // Check to see if test_spot is a distinct value:
                    for (is_distinct = 1, j = 0; j < kNumChannels; j++)
                    {
                        //...if the test spot is already assigned to a locked or stationary channel (2), or a channel we already spread'ed, or a blocked frequency then try again
                        if ((i != j && ((test_spot == test_motion[j] && j < i) || (test_spot == motion_fadeto_note[j] && (lock[j] == 1 || j == 2)))) || (freqblock & (1u << test_spot)))
                            is_distinct = 0;
                    }
                    k += 1;
                }

                test_motion[i] = test_spot;
            }
        }

        motion_spread_dest[0] = (int8_t) test_motion[0];
        motion_spread_dest[1] = (int8_t) test_motion[1];
        motion_spread_dest[2] = (int8_t) test_motion[2];
        motion_spread_dest[3] = (int8_t) test_motion[3];
        motion_spread_dest[4] = (int8_t) test_motion[4];
        motion_spread_dest[5] = (int8_t) test_motion[5];
    }

    // rotation.c update_morph
    void Engine::update_morph()
    {
        const Tables& tb = tables();
        uint16_t chan;

        f_morph *= 0.999f;
        f_morph += 0.001f * (tb.exp_4096[adc_buffer[MORPH_ADC]] / 16.0f);

        //if morph is happening, continue it
        //if it hits the limit, just hold it there until we can run update_motion()
        for (chan = 0; chan < kNumChannels; chan++)
        {
            if (motion_morphpos[chan] > 0.0f)
                motion_morphpos[chan] += f_morph;

            if (motion_morphpos[chan] >= 1.0f) motion_morphpos[chan] = 1.0f;
        }
    }

    // rotation.c update_motion
    void Engine::update_motion()
    {
        uint16_t chan;
        uint16_t test_chan;
        uint8_t is_distinct;
        int steps;

        for (chan = 0; chan < kNumChannels; chan++)
        {
            //if morph has reached the end, shift our present position to the (former) fadeto destination
            if (motion_morphpos[chan] >= 1.0f)
            {
                note[chan] = (uint8_t) motion_fadeto_note[chan];
                scale[chan] = (uint8_t) motion_fadeto_scale[chan];

                if (motion_spread_dest[chan] == note[chan])
                    motion_spread_dir[chan] = 0;

                motion_morphpos[chan] = 0.0f;

                // flag_update_LED_ring = 1: LED only, omitted
            }
        }


        //Initiate a motion if one is not happening
        if (! is_morphing())
        {
            //Rotation CW
            if (motion_rotate > 0)
            {
                motion_rotate--;
                for (chan = 0; chan < 6; chan++)
                {
                    if (! lock[chan])
                    {
                        motion_spread_dir[chan] = 1;
                        //Increment circularly
                        if (motion_spread_dest[chan] >= (kNumFilts - 1))
                            motion_spread_dest[chan] = 0;
                        else
                            motion_spread_dest[chan]++;
                    }
                }
            }
            else

            //Rotation CCW
            if (motion_rotate < 0)
            {
                motion_rotate++;
                for (chan = 0; chan < 6; chan++)
                {
                    if (! lock[chan])
                    {
                        motion_spread_dir[chan] = -1;
                        //Dencrement circularly
                        if (motion_spread_dest[chan] == 0)
                            motion_spread_dest[chan] = kNumFilts - 1;
                        else
                            motion_spread_dest[chan]--;
                    }
                }
            }
            else

            //Rotate CV (jump)
            if (motion_notejump != 0)
            {
                for (chan = 0; chan < 6; chan++)
                {
                    if (! lock[chan])
                    {
                        //Dec/Increment circularly
                        motion_fadeto_note[chan] = (int8_t) (motion_fadeto_note[chan] + motion_notejump);
                        while (motion_fadeto_note[chan] < 0) motion_fadeto_note[chan] = (int8_t) (motion_fadeto_note[chan] + kNumFilts);
                        while (motion_fadeto_note[chan] >= kNumFilts) motion_fadeto_note[chan] = (int8_t) (motion_fadeto_note[chan] - kNumFilts);

                        //Dec/Increment circularly
                        motion_spread_dest[chan] = (int8_t) (motion_spread_dest[chan] + motion_notejump);
                        while (motion_spread_dest[chan] < 0) motion_spread_dest[chan] = (int8_t) (motion_spread_dest[chan] + kNumFilts);
                        while (motion_spread_dest[chan] >= kNumFilts) motion_spread_dest[chan] = (int8_t) (motion_spread_dest[chan] - kNumFilts);


                        //Check if the new destination is occupied by a locked channel, a channel we already notejump'ed or a blocked freq
                        if (freqblock & (1u << motion_fadeto_note[chan]))
                        {
                            is_distinct = 0;
                        }
                        else
                        {
                            for (is_distinct = 1, test_chan = 0; test_chan < kNumChannels; test_chan++)
                            {
                                if (chan != test_chan && motion_fadeto_note[chan] == motion_fadeto_note[test_chan] && (lock[test_chan] || test_chan < chan))
                                    is_distinct = 0;
                            }
                        }
                        steps = 0;
                        while (! is_distinct)
                        {
                            if (steps++ >= kMaxSearchSteps) { ++loop_guard_trips; break; }

                            //Inc/decrement circularly

                            if (motion_notejump > 0)
                            {
                                motion_fadeto_note[chan] = (int8_t) ((motion_fadeto_note[chan] + 1) % kNumFilts);
                            }
                            else
                            {
                                if (motion_fadeto_note[chan] == 0) motion_fadeto_note[chan] = kNumFilts - 1;
                                else motion_fadeto_note[chan] = (int8_t) (motion_fadeto_note[chan] - 1);
                            }

                            //Check if the new destination is occupied by a locked channel, a channel we already notejump'ed or a blocked freq
                            if (freqblock & (1u << motion_fadeto_note[chan]))
                            {
                                is_distinct = 0;
                            }
                            else
                            {
                                for (is_distinct = 1, test_chan = 0; test_chan < kNumChannels; test_chan++)
                                {
                                    if (chan != test_chan && motion_fadeto_note[chan] == motion_fadeto_note[test_chan] && (lock[test_chan] || test_chan < chan))
                                        is_distinct = 0;
                                }
                            }
                        }
                        //Start the motion morph
                        motion_morphpos[chan] = f_morph;
                        motion_fadeto_scale[chan] = motion_scale_dest[chan];
                    }
                }
                motion_notejump = 0;
            }
        }


        for (chan = 0; chan < kNumChannels; chan++)
        {
            if (motion_morphpos[chan] == 0.0f)
            {
                //Spread
                if (motion_spread_dest[chan] != note[chan])
                {
                    //Check if the spread destination is still available
                    if (freqblock & (1u << motion_spread_dest[chan]))
                    {
                        is_distinct = 0;
                    }
                    else
                    {
                        for (is_distinct = 1, test_chan = 0; test_chan < kNumChannels; test_chan++)
                        {
                            if (chan != test_chan && (motion_spread_dest[chan] == motion_spread_dest[test_chan]) && (lock[test_chan] == 1 || test_chan < chan || motion_spread_dir[test_chan] == 0))
                                is_distinct = 0;
                        }
                    }
                    steps = 0;
                    while (! is_distinct)
                    {
                        if (steps++ >= kMaxSearchSteps) { ++loop_guard_trips; break; }

                        //Inc/decrement circularly
                        if (motion_spread_dir[chan] == 0)
                            motion_spread_dir[chan] = 1;

                        if (motion_spread_dir[chan] > 0)
                        {
                            motion_spread_dest[chan] = (int8_t) ((motion_spread_dest[chan] + 1) % kNumFilts);
                        }
                        else
                        {
                            if (motion_spread_dest[chan] == 0) motion_spread_dest[chan] = kNumFilts - 1;
                            else motion_spread_dest[chan] = (int8_t) (motion_spread_dest[chan] - 1);
                        }

                        //Check that it's not already taken by a locked channel, channel with a higher priority, a blocked freq or a non-moving channel
                        if (freqblock & (1u << motion_spread_dest[chan]))
                        {
                            is_distinct = 0;
                        }
                        else
                        {
                            for (is_distinct = 1, test_chan = 0; test_chan < kNumChannels; test_chan++)
                            {
                                if (chan != test_chan && (motion_spread_dest[chan] == motion_spread_dest[test_chan]) && (lock[test_chan] == 1 || test_chan < chan || motion_spread_dir[test_chan] == 0))
                                    is_distinct = 0;
                            }
                        }
                    }


                    //Clockwise Spread
                    if (motion_spread_dir[chan] > 0)
                    {
                        //Start the motion morph
                        motion_morphpos[chan] = f_morph;

                        // Shift the destination CW, wrapping it around
                        is_distinct = 0;
                        steps = 0;
                        while (! is_distinct)
                        {
                            if (steps++ >= kMaxSearchSteps) { ++loop_guard_trips; break; }

                            //Increment circularly
                            if (motion_fadeto_note[chan] >= (kNumFilts - 1))
                            {
                                motion_fadeto_note[chan] = 0;

                                // If scale rotation is on, increment the scale, wrapping it around
                                if (rotate_to_next_scale)
                                {
                                    motion_fadeto_scale[chan] = (int8_t) ((motion_fadeto_scale[chan] + 1) % kNumScales);
                                    motion_scale_dest[chan] = (int8_t) ((motion_scale_dest[chan] + 1) % kNumScales);
                                }
                            }
                            else
                                motion_fadeto_note[chan]++;

                            //Check that it's not already taken by a locked channel, channel with a higher priority, a blocked freq, or a non-moving channel
                            if (freqblock & (1u << motion_fadeto_note[chan]))
                            {
                                is_distinct = 0;
                            }
                            else
                            {
                                for (is_distinct = 1, test_chan = 0; test_chan < kNumChannels; test_chan++)
                                {
                                    if (chan != test_chan && (motion_fadeto_note[chan] == motion_fadeto_note[test_chan]) && (lock[test_chan] == 1 || test_chan < chan))
                                        is_distinct = 0;
                                }
                            }
                        }
                    }
                    else

                    //Counter-clockwise Spread
                    if (motion_spread_dir[chan] < 0)
                    {
                        //Start the motion morph
                        motion_morphpos[chan] = f_morph;

                        // Shift the destination CCW, wrapping it around and repeating until we find a distinct value
                        is_distinct = 0;
                        steps = 0;
                        while (! is_distinct)
                        {
                            if (steps++ >= kMaxSearchSteps) { ++loop_guard_trips; break; }

                            //Decrement circularly
                            if (motion_fadeto_note[chan] == 0)
                            {
                                motion_fadeto_note[chan] = kNumFilts - 1;

                                // If scale rotation is on, decrement the scale, wrapping it around
                                if (rotate_to_next_scale)
                                {
                                    if (motion_fadeto_scale[chan] == 0) motion_fadeto_scale[chan] = kNumScales - 1;
                                    else motion_fadeto_scale[chan]--;
                                    if (motion_scale_dest[chan] == 0) motion_scale_dest[chan] = kNumScales - 1;
                                    else motion_scale_dest[chan]--;
                                }
                            }
                            else
                                motion_fadeto_note[chan]--;

                            //Check that it's not already taken by a locked channel, channel with a higher priority, a blocked freq, or a non-moving channel
                            if (freqblock & (1u << motion_fadeto_note[chan]))
                            {
                                is_distinct = 0;
                            }
                            else
                            {
                                for (is_distinct = 1, test_chan = 0; test_chan < kNumChannels; test_chan++)
                                {
                                    if (chan != test_chan && (motion_fadeto_note[chan] == motion_fadeto_note[test_chan]) && (lock[test_chan] == 1 || test_chan < chan))
                                        is_distinct = 0;
                                }
                            }
                        }
                    }
                }
                else

                //Scale
                if (motion_scale_dest[chan] != motion_fadeto_scale[chan])
                {
                    //Start the motion morph
                    motion_morphpos[chan] = f_morph;

                    motion_fadeto_scale[chan] = motion_scale_dest[chan];
                }
            }
        }
    }
}
