import io
import subprocess
import pprint

import mir_eval
import mirdata
import numpy as np
import sox
import matplotlib.pyplot as plt
import pandas as pd

def bacf(audio_path):
    sr = sox.file_info.sample_rate(audio_path)
    output = subprocess.run(['./cmake-build-debug/BacfEval_artefacts/Debug/BacfEval', f"--path=\"{audio_path}\"", "--sigcond", "--high=600"], capture_output=True, text=True).stdout
    
    if not output:
        return [], []

    result = np.loadtxt(io.StringIO(output), delimiter=',')
    # output of scripts isn't guaranteed to be sorted properly
    print(audio_path)
    print(result.shape)
    est_times, est_freqs = result[np.argsort(result[:, 0])].T
    est_times = (1/sr) * est_times
    return est_times, est_freqs

vocadito = mirdata.initialize('vocadito', data_home="/Users/xavriley/Dropbox/PhD/Datasets/vocadito")
vocadito.download()

# vocadito.validate()

vocadito_scores = {}
show_graphs = True
max_pitch = 0
min_pitch = 1000

for track_id in sorted(vocadito.track_ids):
    track = vocadito.track(track_id)
    est_times, est_freqs = bacf(track.audio_path)

    if(len(est_times) == 0):
        print(f"WARNING: invalid results for file {track_id} at {track.audio_path}")
        continue

    ref_melody_data = track.f0
    ref_times = ref_melody_data.times
    ref_freqs = ref_melody_data.frequencies

    # Track pitch range over the dataset
    max_for_track = np.max(ref_freqs)
    min_for_track = np.min(ref_freqs[np.nonzero(ref_freqs)])
    if(max_for_track > max_pitch):
        max_pitch = max_for_track
    if(min_for_track < min_pitch):
        min_pitch = min_for_track

    if show_graphs:
        fig, (ax1, ax2) = plt.subplots(nrows=2, sharey=True, sharex=True)
        ax1.plot(est_times, est_freqs, label="BACF")
        ax2.plot(ref_times, ref_freqs, label="Ground Truth")
        ax1.set_title(f"BACF Vocadito: {track_id}")
        ax2.set_title(f"Reference Vocadito: {track_id}")
        plt.show()
    

    score = mir_eval.melody.evaluate(ref_times, ref_freqs, est_times, est_freqs)
    vocadito_scores[track_id] = score

df = pd.DataFrame(vocadito_scores)
print(df.T.describe())
print(f"Max pitch: {max_pitch} Hz")
print(f"Min pitch: {min_pitch} Hz")

