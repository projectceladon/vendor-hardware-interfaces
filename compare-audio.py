#!/usr/bin/python3

import ffmpeg
import wave
import struct
import os
from tkinter import *
from tkinter import ttk
from tkinter import filedialog
from tkinter import messagebox


def open_mp3_file(mp3_file_path_obj):
    mp3_file_path_obj.set(filedialog.askopenfilename(
        filetypes=[('mp3/aac file', ['*.mp3', '*.aac']), ('All', "*.*")], title='open MP3 file'))


def open_pcm_file(pcm_file_path_obj):
    pcm_file_path_obj.set(filedialog.askopenfilename(
        filetypes=[('pcm file', '*.pcm'), ('All', "*.*")], title='open PCM file'))


def compare_audio(mp3_file_path_obj, pcm_file_path_obj, sample_rate_obj, channels_obj, bit_rate_obj, result_obj):
    if mp3_file_path_obj.get() == '':
        messagebox.showerror(message=".mp3/.aac file shouldn't be empty!")
        return

    if pcm_file_path_obj.get() == '':
        messagebox.showerror(message=".pcm file shouldn't be empty!")
        return

    try:
        pcm_sample_rate = sample_rate_obj.get()
        pcm_channel_num = channels_obj.get()
        pcm_bit_rate = bit_rate_obj.get()
    except Exception:
        messagebox.showerror(message="Invalid audio parameters for PCM file!")
        return

    # Decode and convert .mp3/.aac file to .wav file
    stream_in = ffmpeg.input(mp3_file_path_obj.get())
    wav_audio_filename = mp3_file_path_obj.get().rsplit('.', 1)[0] + '.wav'
    stream_out = ffmpeg.output(stream_in, wav_audio_filename)
    ffmpeg.run(stream_out, overwrite_output=True)
    print("Successfully convert .mp3/.aac file to .wav file!")

    # Get the .wav file codec name before converting to PCM int list
    try:
        wav_metadata = ffmpeg.probe(wav_audio_filename)
    except ffmpeg.Error as e:
        messagebox.showerror(
            message="Failed to get the audio parameters from the intermediate .wav file")
        result_obj.set("Fail")
        return

    wav_nb_streams = wav_metadata['format']['nb_streams']
    if wav_nb_streams != 1:
        messagebox.showerror(
            message="Number of audio streams should be 1 for the intermediate .wav file")
        result_obj.set("Fail")
        return

    wav_codec_name = wav_metadata['streams'][0]['codec_name']
    wav_codec_name_suffix = wav_codec_name.split('pcm_')[1]

    if wav_codec_name_suffix.startswith('f'):
        wav_pcm_data_float = True
    elif wav_codec_name_suffix.startswith('dvd'):
        wav_pcm_data_float = False
        wav_pcm_data_signed = True
    elif wav_codec_name_suffix.startswith('s'):
        wav_pcm_data_float = False
        wav_pcm_data_signed = True
    elif wav_codec_name_suffix.startswith('u'):
        wav_pcm_data_float = False
        wav_pcm_data_signed = False
    else:
        messagebox.showerror(
            message="{} encoder for .wav file is not supported".format(wav_codec_name))
        result_obj.set("Fail")
        return

    wav_pcm_data_native = False
    if 'be' in wav_codec_name_suffix:
        wav_pcm_data_little_endian = False
    elif 'le' in wav_codec_name_suffix:
        wav_pcm_data_little_endian = True
    else:
        wav_pcm_data_native = True

    # Abstract PCM sequence and related audio parameters from .wav file
    wav_formatted_data_list = list()
    with wave.open(wav_audio_filename, 'rb') as wav_file:
        wav_frame_num = wav_file.getnframes()
        wav_channel_num = wav_file.getnchannels()
        wav_sample_rate = wav_file.getframerate()
        wav_sample_width = wav_file.getsampwidth()
        wav_bit_rate = (wav_sample_width * 8) * \
            wav_channel_num * wav_sample_rate

        # Parse .wav file audio parameters and compare with the input ones
        if pcm_channel_num != wav_channel_num:
            messagebox.showerror(message="Channel numbers of .pcm file is {}, different from {} in the intermediate .wav file".format(pcm_channel_num, wav_channel_num))
            result_obj.set("Fail")
            return

        if pcm_bit_rate != wav_bit_rate:
            messagebox.showerror(message="Bit rate of .pcm file is {}, different from {} in the intermediate .wav file".format(pcm_bit_rate, wav_bit_rate))
            result_obj.set("Fail")
            return

        if pcm_sample_rate != wav_sample_rate:
            messagebox.showerror(message="Sample rate of .pcm file is {}, different from {} in the intermediate .wav file".format(pcm_sample_rate, wav_sample_rate))
            result_obj.set("Fail")
            return

        # Convert PCM stream to the int list
        wav_format_string = ""
        if wav_pcm_data_native:
            wav_format_string = wav_format_string + '='
        elif wav_pcm_data_little_endian:
            wav_format_string = wav_format_string + '<'
        else:
            wav_format_string = wav_format_string + '>'

        if wav_pcm_data_float:
            if wav_sample_width == 4:
                wav_format_string = wav_format_string + 'f'
            elif wav_sample_width == 8:
                wav_format_string = wav_format_string + 'd'
            else:
                messagebox.showerror(message="Sample width {} and encoder {} of the intermediate .wav file mismatch".format(wav_sample_width, wav_codec_name))
                result_obj.set("Fail")
                return
        else:
            if wav_sample_width == 1:
                format_string_suffix = 'b'
            elif wav_sample_width == 2:
                format_string_suffix = 'h'
            elif wav_sample_width == 4:
                format_string_suffix = 'l'
            elif wav_sample_width == 8:
                format_string_suffix = 'q'
            else:
                messagebox.showerror(message="Sample width {} and encoder {} of the intermediate .wav file mismatch".format(wav_sample_width, wav_codec_name))
                result_obj.set("Fail")

        if wav_pcm_data_signed:
            wav_format_string = wav_format_string + format_string_suffix
        else:
            wav_format_string = wav_format_string + format_string_suffix.upper()
        print("Format string for the intermediate .wav file is {}".format(wav_format_string))

        for i in range(0, wav_frame_num):
            wav_frame_bytes = wav_file.readframes(1)
            for j in range(0, wav_channel_num):
                start_index = j * wav_sample_width
                end_index = (j + 1) * wav_sample_width
                wav_frame_channel_data = struct.unpack(wav_format_string, wav_frame_bytes[start_index : end_index])
                wav_formatted_data_list.append(wav_frame_channel_data[0])

    # Encoding format of the PCM file is 16-bit little-endian (signed) integer
    pcm_sample_width = wav_sample_width
    pcm_format_string = wav_format_string
    pcm_formatted_data_list = list()
    with open(pcm_file_path_obj.get(), 'rb') as pcm_file:
        pcm_contents = pcm_file.read()
        pcm_frame_size = pcm_channel_num * pcm_sample_width
        pcm_frame_num = len(pcm_contents) / pcm_frame_size

        if pcm_frame_num != wav_frame_num:
            messagebox.showerror(message="Different frame number of intermediate WAV file({}) and raw PCM file({})!".format(wav_frame_num, pcm_frame_num))
            result_obj.set("Fail")

        for i in range(0, int(pcm_frame_num)):
            pcm_frame_bytes = pcm_contents[i * pcm_frame_size : (i + 1) * pcm_frame_size]
            for j in range(0, pcm_channel_num):
                start_index = j * pcm_sample_width
                end_index = (j + 1) * pcm_sample_width
                pcm_frame_channel_data = struct.unpack(pcm_format_string, pcm_frame_bytes[start_index : end_index])
                pcm_formatted_data_list.append(pcm_frame_channel_data[0])

    # Measure similarity of `wav_formatted_data_list` and `pcm_formatted_data_list`
    squared_error = 0.0
    mismatch_map = dict() # Key: (frame, channel) as tuple Value: (pcm, wav) as tuple
    for i in range(0, wav_frame_num):
        wav_formatted_frame_data = wav_formatted_data_list[i * wav_channel_num : (i + 1) * wav_channel_num]
        pcm_formatted_frame_data = pcm_formatted_data_list[i * pcm_channel_num : (i + 1) * pcm_channel_num]
        for j in range(0, wav_channel_num):
            wav_frame_channel_data = wav_formatted_frame_data[j]
            pcm_frame_channel_data = pcm_formatted_frame_data[j]
            if wav_frame_channel_data != pcm_frame_channel_data:
                mismatch_map.update({(i, j): (pcm_frame_channel_data, wav_frame_channel_data)})
                squared_error = squared_error + pow(abs(pcm_frame_channel_data - wav_frame_channel_data), 2)

    diff_file_path = pcm_file_path_obj.get().rsplit('.', 1)[0] + '-' + \
                     os.path.basename(wav_audio_filename).rsplit('.', 1)[0] + '.diff'
    with open (diff_file_path, 'w') as diff_file:
        content = "Sample Rate: {}; Total Channel: {}; Sample Width: {}; Total Frame: {}; Total Difference: {}\n".format(
                  wav_sample_rate, wav_channel_num, wav_sample_width, wav_frame_num, len(mismatch_map))
        diff_file.write(content)
        for dict_key in mismatch_map:
            frame_id = dict_key[0]
            channel_id = dict_key[1]
            pcm_value = mismatch_map[dict_key][0]
            wav_value = mismatch_map[dict_key][1]
            content = "Frame: {}; Channel: {}; Value: {}; Reference: {}\n".format(frame_id, channel_id, wav_value,
                      pcm_value)
            diff_file.write(content)

    rmse = pow(squared_error / (pcm_frame_num * wav_channel_num), 0.5)
    if wav_pcm_data_float:
        if wav_sample_width == 4:
            max_value = "3.40e+38"
            min_value = "-3.40e+38"
        else:
            max_value = "1.80e+308"
            min_value = "-1.80e+308"
        print("RMSE: {:.2e}; Value Scope: {} - {}".format(rmse, min_value, max_value))
    else:
        if wav_pcm_data_signed:
            max_value = pow(2, wav_sample_width * 8 - 1) - 1
            min_value = -pow(2, wav_sample_width * 8 - 1)
        else:
            max_value = pow(2, wav_sample_width * 8) - 1
            min_value = -pow(2, wav_sample_width * 8)
        print("RMSE: {:.2e}; Value Scope: {:.2e} - {:.2e}".format(rmse, min_value, max_value))

    if len(mismatch_map) == 0:
        result_obj.set("Pass")
    else:
        result_obj.set("Fail")
        messagebox.showinfo(message="RMSE: {:.2e}; Value Scope: [{:.2e}, {:.2e}]\n" \
                                     "All the differences are listed in {}".format(rmse, min_value, max_value,
                                     diff_file_path))

def main():
    root = Tk()
    root.title("Comparing MP3/AAC and PCM")

    mainFrame = ttk.Frame(root, padding="8 8 8 8")
    mainFrame.grid(row=0, column=0, sticky=(N, W, E, S))

    mp3_file_path_obj = StringVar()
    pcm_file_path_obj = StringVar()
    sample_rate_obj = IntVar()
    channels_obj = IntVar()
    bit_rate_obj = IntVar()
    result_obj = StringVar()

    mp3_entry = ttk.Entry(mainFrame, textvariable=mp3_file_path_obj)
    pcm_entry = ttk.Entry(mainFrame, textvariable=pcm_file_path_obj)
    mp3_entry.grid(row=0, column=0, columnspan=4, sticky=(W, E))
    pcm_entry.grid(row=1, column=0, columnspan=4, sticky=(W, E))

    mp3_button = ttk.Button(
        mainFrame, text='Open MP3/AAC file', command=lambda: open_mp3_file(mp3_file_path_obj))
    pcm_button = ttk.Button(
        mainFrame, text='Open PCM file', command=lambda: open_pcm_file(pcm_file_path_obj))
    mp3_button.grid(row=0, column=4)
    pcm_button.grid(row=1, column=4)

    ttk.Label(mainFrame, text="Sample rate").grid(row=2, column=0)
    sample_rate_entry = ttk.Entry(mainFrame, textvariable=sample_rate_obj)
    sample_rate_entry.grid(row=2, column=2, columnspan=2)

    ttk.Label(mainFrame, text="Channels").grid(row=3, column=0)
    channels_entry = ttk.Entry(mainFrame, textvariable=channels_obj)
    channels_entry.grid(row=3, column=2, columnspan=2)

    ttk.Label(mainFrame, text="Bit rate").grid(row=4, column=0)
    bit_rate_entry = ttk.Entry(mainFrame, textvariable=bit_rate_obj)
    bit_rate_entry.grid(row=4, column=2, columnspan=2)

    compare_button = ttk.Button(
        mainFrame, text="Compare", command=lambda: compare_audio(mp3_file_path_obj, pcm_file_path_obj, sample_rate_obj, channels_obj, bit_rate_obj, result_obj))
    compare_button.grid(row=2, column=4, rowspan=2)

    result_label = ttk.Label(mainFrame, textvariable=result_obj)
    result_label.grid(row=4, column=4)

    compare_button.focus()
    root.bind("<Return>", func=lambda: compare_audio(mp3_file_path_obj,
              pcm_file_path_obj, sample_rate_obj, channels_obj, bit_rate_obj, result_obj))

    root.mainloop()


if __name__ == "__main__":
    os.sys.exit(main())
