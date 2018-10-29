#!/usr/bin/env ruby
#
# Part of Metta OS. Check https://metta.systems for latest version.
#
# Copyright Stanislav Karchebnyy <berkus+github@metta.systems>
#
# Distributed under the Boost Software License, Version 1.0.
# (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Apply license changes to text source files.
# Run as ./apply_boilerplate.rb
#
require 'find'

exclude_dirs = ['./_build_', './_conan_build_', './Telegram/ThirdParty']
# Files that shouldn't update license headers
no_license = [
    './apply_boilerplate.rb',
    './Telegram/Resources/winrc/resource.h',
    './Telegram/SourceFiles/qt_functions.cpp',
    './Telegram/SourceFiles/backports/is_invocable.h',
    './Telegram/SourceFiles/boxes/mute_settings_box.h',
    './Telegram/SourceFiles/boxes/mute_settings_box.cpp'
]

class Array
    def do_not_has?(path)
        count {|x| path.start_with?(x)} === 0
    end
end

license = IO.readlines('license_header').join
orig_license = IO.readlines('original_license_header').join
exts = {
    '.cpp'=>[license, orig_license],
    '.c'=>[license, orig_license],
    '.h'=>[license, orig_license],
    '.m'=>[license, orig_license],
    '.mm'=>[license, orig_license],
    '.strings'=>[license, orig_license],
    '.style'=>[license, orig_license],
    '.s'=>[license.gsub(/^\/\//,";"), orig_license],
    '.rb'=>[license.gsub(/^\/\//,"#"), orig_license],
    '.lua'=>[license.gsub(/^\/\//,"--"), orig_license],
    '.if'=>[license.gsub(/^\/\//,"#"), orig_license]
}

ok_count = 0
modified_count = 0
modified_files = []

Find.find('./') do |f|
    ext = File.extname(f)
    dir = File.dirname(f)
    if File.file?(f) && exts.include?(ext) && exclude_dirs.do_not_has?(dir)
        lic = exts[ext][0]
        orig = exts[ext][1]
        modified = false
        content = IO.readlines(f).join
        if !content.index(orig).nil?
            # Strip-off original license header
            content.gsub!(orig, '')
        end
        if content.index(lic).nil? && no_license.do_not_has?(dir) && no_license.do_not_has?(f)
            content = lic + content
            modified = true
        end
        if modified
            File.open(f+".new", "w") do |out|
                out.write content
            end
            begin
                File.rename(f+".new", f)
            rescue SystemCallError
                puts "Couldn't rename file #{f+".new"} to #{f}:", $!
            end
            puts "#{f} is UPDATED"
            modified_count += 1
            modified_files << f
        else
            puts "#{f} is ok"
            ok_count += 1
        end
    end
end

puts "#{modified_count} files changed, #{ok_count} files ok."
unless modified_files.empty?
    puts "Modified files:"
    modified_files.each { |f| puts f }
end
