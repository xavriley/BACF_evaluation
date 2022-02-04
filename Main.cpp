#include <fstream>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <q/support/literals.hpp>
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/signal_conditioner.hpp>

void detectPitch (juce::ArgumentList args) {
    namespace q = cycfi::q;
    using namespace q::literals;

    using namespace juce;

    auto file = args.getExistingFileForOption("--path");
    auto lowest_freq_option = args.getValueForOption("--low");
    auto highest_freq_option = args.getValueForOption("--high");
    auto use_sigcond = args.containsOption("--sigcond");

    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    AudioFormatReader *reader = formatManager.createReaderFor(file);

    AudioSampleBuffer buffer;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
    buffer.setSize(static_cast<int>(reader->numChannels), reader->lengthInSamples);
    const auto sps = static_cast<std::uint32_t>(reader->sampleRate);
    reader->read(&buffer, 0, reader->lengthInSamples, 0, true, true);
#pragma clang diagnostic pop
    delete reader;

    // primitive normalisation
    // auto max_sample = buffer.getMagnitude(0, buffer.getNumSamples());
    // auto orig_rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    // auto orig_db = Decibels::gainToDecibels(orig_rms);
    // buffer.applyGain(Decibels::decibelsToGain(abs(orig_db)/2));
    // auto new_rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());

    // Pitch detection
    q::frequency lowest_freq = lowest_freq_option.isNotEmpty() ? q::frequency{ lowest_freq_option.getDoubleValue() } : 60_Hz;
    q::frequency highest_freq = highest_freq_option.isNotEmpty() ? q::frequency{ highest_freq_option.getDoubleValue() } : lowest_freq * 10;

    q::pitch_detector          pd{ lowest_freq, highest_freq, sps, -40_dB };

    // Signal conditioner
    auto sc_conf = q::signal_conditioner::config{};
    sc_conf.pre_clip_level = 0_dB;
    sc_conf.comp_threshold = 0_dB;
    sc_conf.gate_onset_threshold = -42_dB;
    sc_conf.gate_release_threshold = -57_dB ;

    auto sig_cond = q::signal_conditioner{sc_conf, lowest_freq, highest_freq, sps};

    auto _env = q::fast_envelope_follower{lowest_freq.period() * 0.6, sps};
    auto _gate = q::basic_noise_gate<50>{sc_conf.gate_onset_threshold, sc_conf.gate_release_threshold, sc_conf.attack_width, sps};
    auto _gate_env = q::envelope_follower{500_us, sc_conf.gate_release, sps};

    auto *samples = buffer.getWritePointer(0);

    // avoid blank output
    std::cout << 0 << ", " << 0 << std::endl;
    std::cout << 0 << ", " << 0 << std::endl;

    auto prev_frequency = 0;

    for (auto sample = 0; sample < buffer.getNumSamples(); sample++) {
        auto s = samples[sample];

        if(use_sigcond) {
            auto env = _env(std::abs(s));
            auto gate = _gate(env);
            s *= _gate_env(gate);
            samples[sample] = s;

            // samples[sample] = sig_cond(s);
            // s = samples[sample];
        }

        bool is_ready = pd(s);

        if(is_ready) {
            auto frequency = pd.get_frequency();
            auto window_size = pd.edges().window_size();
            auto frame = pd.edges().frame();
            auto periodicity = pd.periodicity();
            // maybe subtract frequency period in samples as well
            auto start_time = (sample - window_size) + frame - float(sps / q::as_float(lowest_freq));

            // if(periodicity > 0.5f) {
                // If a new note is detected after a period of silence or non-periodicity
                // we mark it by writing a zero one sample before. This is to avoid issues with
                // evaluation tools doing linear interpolation between the older zero and the new frequency
                if(prev_frequency < 1 && frequency > 0)
                    std::cout << int(start_time - 1) << ", " << 0 << std::endl;

                std::cout << int(start_time) << ", " << frequency << std::endl;
            // } else {
            // std::cout << int(start_time) << ", " << 0 << std::endl;
            //}

            prev_frequency = frequency;
        }

        std::cout.flush();
    }

    std::cout << std::endl;

    auto parentDir = File::getCurrentWorkingDirectory();
    File lastRecording = parentDir.getChildFile ("JUCE Demo Audio Recording.wav");

    WavAudioFormat format;
    std::unique_ptr<AudioFormatWriter> writer;
    writer.reset (format.createWriterFor (new FileOutputStream (lastRecording),
                                          44100.0,
                                          buffer.getNumChannels(),
                                          24,
                                          {},
                                          0));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}


int main (int argc, char* argv[]) {
    using namespace juce;

    ConsoleApplication app;

    app.addCommand({"--path",
                    "--path=\"/path/to/audio/file\"",
                    "Runs pitch detection on file",
                    "This runs the bitstream autocorrelation function (BACF) on this file, outputting a sample index and a pitch prediction in Hz",
                    detectPitch});
    app.addCommand({"--low",
                    "--low=60",
                    "Lowest detectable pitch (in Hz)",
                    "Lowest pitch detectable by BACF. Lower pitches mean longer analysis windows, which affects the spacing of the pitch estimates.",
                    detectPitch});
    app.addCommand({"--high",
                    "--high=1500",
                    "Highest detectable pitch (in Hz)",
                    "Highest pitch detectable by BACF.",
                    detectPitch});
    app.addCommand({"--sigcond",
                    "--sigcond",
                    "Enable signal conditioner",
                    "Enables signal conditioner. This adds effects such as dynamic smoothing, noise gating and compression. Off by default.",
                    detectPitch});

    return app.findAndRunCommand(argc, argv);
}
