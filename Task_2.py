## Mocking Bot - Task 2 : Instrument Classification

#  Instructions
#  ------------
#
#  This file contains Main function and Instrument_identify function. Main Function helps you to check your output
#  for practice audio files provided. Do not make any changes in the Main Function.
#  You have to complete only the Instrument_identify function. You can add helper functions but make sure
#  that these functions are called from Instrument_identify function. The final output should be returned
#  from the Instrument_identify function.
#
#  Note: While evaluation we will use only the onset_detect function. Hence the format of input, output
#  or returned arguments should be as per the given format.
#  
#  Recommended Python version is 2.7.
#  The submitted Python file must be 2.7 compatible as the evaluation will be done on Python 2.7.
#  
#  Warning: The error due to compatibility will not be entertained.
#  -------------


## Library initialisation

# Import Modules
# DO NOT import any library/module
# related to Audio Processing here
import numpy as np
import math
import wave
import os
import struct
import serial
import time

from scipy.fftpack import fft
from joblib import load  # to load a trained model.
import scipy.io.wavfile
from pyAudioAnalysis import audioFeatureExtraction  # tool used for Feature Extraction


# Teams can add helper functions
# Add all helper functions here

# Function will take sound and onsets(in terms of start and end)as arguments.
# Classification of instruments is done by Feature Extraction, and then using a trained model.
def predict(sound, start, end):
	instruments = []  					# stores the return instrument_names.
	features = []						# stores all the extracted features

	Sampling_frequency = 44100
	for i in range(0, len(start)):
		note = sound[int(start[i] * Sampling_frequency): int(end[i] * Sampling_frequency)]  												# stores amplitudes for individual notes
		F, feature_names = audioFeatureExtraction.stFeatureExtraction(note, Sampling_frequency, 0.05 * Sampling_frequency, 0.025 * Sampling_frequency)		# Features of note are extracted into F. The list of features used is given in readme
		features.append(F)

	test_features = np.zeros([len(features), 2 * len(feature_names)])  # this will contain the mean and variance values of each extracted feature.

	for j in range(0, len(features)):
		for i in range(0, len(features[j])):
			test_features[j][i] = np.mean(features[j][i])  				# first 34 features will be mean.
			test_features[j][i + 34] = np.var(features[j][i])  			# last 34 features will be variance


	classifier = load('classifier.joblib')  					# "  classifier.joblib" contains the trained model, which is loaded into 'classifier'

	instruments = list(classifier.predict(test_features))  # prediction

	return instruments


def notes(sound, start, end):  		# returns the notes
	note = []
	for i in range(0, len(start)):
		x = sound[int(start[i] * 44100): int(end[i] * 44100)]  # stores amplitudes for individual notes
		note.append(note_detect(x))  			# call note_detect to detect individual notes
	return note


def ispeak(f, it):  		# finds peek amidst a range of ten values
	for i in range(it - 10, it + 10):
		if (f[it] < f[i]):
			return 0
	return 1


def round_off(v):
	fraction = abs(v - int(v))
	if (v < 0):
		return int((fraction < 0.5) * math.ceil(v) + (fraction >= 0.5) * math.floor(v))
	else:
		return int((fraction >= 0.5) * math.ceil(v) + (fraction < 0.5) * math.floor(v))


# note_detection is same as the one used in previous Tasks
def note_detect(sound):
	notes = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']  # List of all Notes

	file_length = len(sound)

	f = fft(sound)
	f = np.abs(f)
	max_amp = np.max(f)

        i_max = np.argmax(abs(f))
	if(file_length < 0.5 * 44100):
		off_set = round_off(12 * math.log(i_max * 44100 / file_length / 440.0, 2))
	else:
		for it in range(int(0.001*len(f)), len(f)):
			if (f[it] > 0.09 * max_amp and ispeak(f, it) == 1):  # threshold is 10% of max
				off_set = round_off(12 * math.log(it * 44100 / file_length / 440.0, 2))  	# off_set with respect to A4 note is found from formula																					# fn = f0 * (a)^n, where a = 2^1/12, n is off_set, f0 = 440Hz
				break
	if (int(off_set < 0)):
		note = str(notes[(9 + off_set) % 12]) + str(4 - int((2 - off_set) / 12))  # to get octave and note from off_set
	else:
		note = str(notes[(9 + off_set) % 12]) + str(4 + int((9 + off_set) / 12))

	Detected_Note = note

	return Detected_Note


############################### Your Code Here #############################################

def Instrument_identify(audio_file):
	#   Instructions
	#   ------------
	#   Input 	:	audio_file -- a single test audio_file as input argument
	#   Output	:	1. Instruments -- List of string corresponding to the Instrument
	#			2. Detected_Notes -- List of string corresponding to the Detected Notes
	#                       3. Onsets -- List of Float numbers corresponding
	#			        to the Note Onsets (up to Two decimal places)
	#   Example	:	For Audio_1.wav file,
	# 				Instruments = ["Piano","Violin","Piano","Trumpet"]
	#				Detected_Notes = ["C3","B5","A6","C5"]
	#                               Onsets = [0.00, 0.99, 1.32, 2.04]

	# Add your code here

	sampling_freq = 44100
	audio_file.setpos(0)
	file_length = audio_file.getnframes()

	sound = np.zeros(file_length)
	for i in range(file_length):
		data = audio_file.readframes(1)
		data = struct.unpack('<h', data)
		sound[i] = int(data[0])
	sound = np.divide(sound, (2 ** 15))

	sound_square = np.square(sound)  				# square each element of sound[]

	window = 0.01 * 44100							# window is 0.25 seconds
	i = 0
	ps = 0
	start = []
	end = []
	start_flag = 0
	sum_treshold = 0.01								#threshold is 25
	while (i < (file_length) - window):				#this loop is used to find start and end times of all notes in single audio file
		s = 0
		j = 0
		while (j <= window):
			s = s + sound_square[int(i + j)]
			j = j + 1
		if (s > sum_treshold and ps < sum_treshold):
			start.append((i) / 44100.0)
			start_flag = 1
		if (s < sum_treshold and ps > sum_treshold and start_flag == 1):
			start_flag = 0
			end.append((i - window) / 44100.0)
		i = i + window
		ps = s

	if (start_flag == 1):							#if the last note is not ended, we need to add file-end as its end
		end.append((i) / 44100.0)
	# print(start, end)
	Instruments = predict(sound, start, end)
	Detected_Notes = notes(sound, start, end)
	Onsets = start
	print(len(start))
	# Send this information to the bot
	Ins = ""
	Start = ""
	End = ""
	Notes = ""
#	dict = {'Flute' : 1, 'Piano' : 2, 'Trumpet' : 3, 'Violin' : 4}
	for i in range(0, len(Instruments)):
		if Instruments[i] == 'Piano' or Instruments[i] == 'Trumpet':						        # piano and trumpet notes with their onsets are sent
                        Ins = Ins + Instruments[i][0]
                        Start = Start + str(int(start[i] * 100)) + ' '
                        End = End + str(int(end[i] * 100)) + ' '
                        Notes = Notes + Detected_Notes[i][0]
        Ins = Ins + '$'
        Start = Start + '$'
        End = End + '$'
        Notes = Notes + '$'
        
	print Ins
	print Notes
	print Start
	print End

	com_port = 'COM5'
	ser = serial.Serial()
	ser.baudrate = 9600
	ser.port = com_port

	print "Port used : " + ser.name
	ser.open()
	print ser.name + " open"

	x = ser.read(1)
	while x != '#':																				# wait till '#' received from bot
		x = ser.read(1)
	print("starting transmission of ins")
	for i in Ins:																			# Transmitting string with delay of 50ms between characters
		time.sleep(0.2) 																	# Without this delay, data is getting lost
		ser.write(i)

        x = ser.read(1)
	while x != '#':																				# wait till '#' received from bot
		x = ser.read(1)
	print("starting transmission of notes")
	for i in Notes:																			# Transmitting string with delay of 50ms between characters
		time.sleep(0.2)																		# Without this delay, data is getting lost
		ser.write(i)

        x = ser.read(1)
	while x != '#':																				# wait till '#' received from bot
		x = ser.read(1)
	print("starting transmission of start times")
	for i in Start:																			# Transmitting string with delay of 50ms between characters
		time.sleep(0.2)																		# Without this delay, data is getting lost
		ser.write(i)

        x = ser.read(1)
	while x != '#':																				# wait till '#' received from bot
		x = ser.read(1)																				# Receive '@' as an acknowledgement from bot
        print("starting transmission of end times")
	for i in End:																			# Transmitting string with delay of 50ms between characters
		time.sleep(0.2)																		# Without this delay, data is getting lost
		ser.write(i)

        print ser.read(1)
        
	return Instruments, Detected_Notes, Onsets


############################### Main Function #############################################

if __name__ == "__main__":

	#   Instructions
	#   ------------
	#   Do not edit this function.

	# code for checking output for single audio file
	path = os.getcwd()

	file_name = path + "/Task_2_Audio_files/Audio_1.wav"
	audio_file = wave.open(file_name)
	
	Instruments, Detected_Notes, Onsets = Instrument_identify(audio_file)

	print("\n\tInstruments = "  + str(Instruments))
	print("\n\tDetected Notes = " + str(Detected_Notes))
	print("\n\tOnsets = " + str(Onsets))
	# code for checking output for all audio files
	
	x = raw_input("\n\tWant to check output for all Audio Files - Y/N: ")
		
	if x == 'Y':

		Instruments_list = []
		Detected_Notes_list = []
		Onsets_list = []

		file_count = len(os.listdir(path + "/Task_2_Audio_files"))

		for file_number in range(1, file_count):

			file_name = path + "/Task_2_Audio_files/Audio_"+str(file_number)+".wav"
			audio_file = wave.open(file_name)

			Instruments, Detected_Notes,Onsets = Instrument_identify(audio_file)
			
			Instruments_list.append(Instruments)
			Detected_Notes_list.append(Detected_Notes)
			Onsets_list.append(Onsets)
		print("\n\tInstruments = " + str(Instruments_list))
		print("\n\tDetected Notes = " + str(Detected_Notes_list))
		print("\n\tOnsets = " + str(Onsets_list))

