package com.vtec.ediwand

import java.util.*

class ControlComputer(val hostname: String, val address: String, var os: OS, var recognized: Date)
{
    enum class OS {
        Linux, Windows, MacOS, SerenityOS, Unknown
    }
}