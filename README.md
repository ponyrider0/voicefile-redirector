# VoiceFile-Redirector OBSE Plugin v1.0

Description:
OBSE plugin to redirect missing voicefiles to other locations.  Based on Elys_USV.
Version 1.0 expands on elys_usv.dll functionallity with the following: If no voicefiles are present, it first tries to substitute the imperial race in the voice filename to find a match.  If none present, it will try to detect if current dialog is a greeting and then redirect to a generic greeting based on race and sex of speaker.  If both fail, it will fallback to elys_usv.mp3 if present.

Requirements:
OBSE v0021

Credit:
Thanks to Elys for supplying original source code and programming suggestions.  Thanks to llde for assistance with building OBSE source code.  And thanks to Morroblivion community for their suggestions, testing and other contributions to this project.
