import json
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]

FIRSTLIGHT_IMPORTS={
    "Ghz9iWDUtC4#libSceNpManager":"sceNpGetAccountCountry",
    "rShmqXHwoQE#libSceNpScore":"sceNpScoreGetRankingByRangeAsync",
    "Rd27dqUFZV8#libSceNpScore":"sceNpScoreGetRankingByNpIdAsync",
    "m1DfNRstkSQ#libSceNpScore":"sceNpScorePollAsync",
    "ANJssPz3mY0#libSceNpScore":"sceNpScoreRecordScoreAsync",
    "CcIH40dYS88#libSceNpTus":"sceNpTusDeleteRequest",
    "3bh2aBvvmvM#libSceNpTus":"sceNpTusCreateRequest",
    "bcPB2rnhQqo#libSceNpTus":"sceNpTusGetMultiSlotVariableAsync",
    "YRje5yEXS0U#libSceNpTus":"sceNpTusGetMultiUserVariableAsync",
    "t7b6dmpQNiI#libSceNpTus":"sceNpTusPollAsync",
    "5J9GGMludxY#libSceNpTus":"sceNpTusSetMultiSlotVariableAsync",
    "BIkMmUfNKWM#libSceNpTus":"sceNpTusCreateNpTitleCtx",
    "H3uq7x0sZOI#libSceNpTus":"sceNpTusDeleteNpTitleCtx",
    "2qSZ0DgwTsc#libSceNpWebApi":"sceNpWebApiGetErrorCode",
    "BtqmpTRXHgk#libSceRtc":"sceRtcGetTime_t",
}

class FirstLightPortTests(unittest.TestCase):
    def test_target_manifest(self):
        target=json.loads((ROOT/"targets"/"CUSA00575.json").read_text(encoding="utf-8"))
        self.assertEqual(target["title_id"],"CUSA00575")
        self.assertEqual(target["app_ver"],"01.00")
        self.assertEqual(target["executable"]["entry_point"],"0xa19c30")
        self.assertEqual(target["executable"]["referenced_imports"],468)
        self.assertEqual(sum(x["tls_relocations"] for x in target["modules"]),16)

    def test_import_name_table(self):
        text=(ROOT/"src"/"import_names.inc").read_text(encoding="utf-8")
        pairs=dict(re.findall(r'\{"([^"]+)","([^"]+)"\},?',text))
        for nid,symbol in FIRSTLIGHT_IMPORTS.items():
            self.assertEqual(pairs.get(nid),symbol,nid)

    def test_offline_np_contracts(self):
        text=(ROOT/"src"/"runtime_services.c").read_text(encoding="utf-8")
        for symbol in list(FIRSTLIGHT_IMPORTS.values())[:-1]:
            self.assertIn(f'"{symbol}"',text)
        self.assertIn("np_webapi_error_code",text)

    def test_rtc_contract_exists(self):
        text=(ROOT/"src"/"runtime_rtc.c").read_text(encoding="utf-8")
        self.assertIn('"sceRtcGetTime_t"',text)

    def test_gow3_gameplay_hooks_not_installed(self):
        text=(ROOT/"src"/"probe.c").read_text(encoding="utf-8")
        for call in (
            "if (gpu_enabled) install_orb_multiplier(",
            "if (gpu_enabled) install_cheats(",
            "if (gpu_enabled) install_actor_watch(",
            "if (gpu_enabled) install_stat_multipliers(",
        ):
            self.assertNotIn(call,text)
        self.assertIn("if (check_imports && cpu_only) gow3gpu_register_graphics_symbols();",text)

    def test_headless_gpu_registration(self):
        gnm=(ROOT/"gpu"/"shadps4"/"core"/"libraries"/"gnmdriver"/"gnmdriver.cpp").read_text(encoding="utf-8")
        video=(ROOT/"gpu"/"shadps4"/"core"/"libraries"/"videoout"/"video_out.cpp").read_text(encoding="utf-8")
        shim=(ROOT/"gpu"/"shim"/"gow3gpu.cpp").read_text(encoding="utf-8")
        self.assertIn("bool initialize",gnm)
        self.assertIn("if (initialize)",gnm)
        self.assertIn("bool initialize",video)
        self.assertIn("if (initialize) {\n        driver = std::make_unique<VideoOutDriver>",video)
        self.assertIn("gow3gpu_register_graphics_symbols",shim)
        self.assertIn("RegisterLib(&resolver, false)",shim)

    def test_firstlight_profile_starts_generic(self):
        profile=(ROOT/"gpu"/"games"/"firstlight.cpp").read_text(encoding="utf-8")
        cmake=(ROOT/"gpu"/"CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('"CUSA00575"',profile)
        self.assertIn(".install_hooks = nullptr",profile)
        self.assertIn(".dynamic_writers = {}",profile)
        self.assertIn(".buffer_copy_shader = 0",profile)
        self.assertIn("FIRSTLIGHT_GAME_PROFILES",cmake)

    def test_shadps4_confirmed_service_and_gpu_names(self):
        names=(ROOT/"src"/"import_names.inc").read_text(encoding="utf-8")
        services=(ROOT/"src"/"runtime_services.c").read_text(encoding="utf-8")
        confirmed={
            "I88KChlynSs#libSceErrorDialog":"sceErrorDialogInitialize",
            "9XAxK2PMwk8#libSceErrorDialog":"sceErrorDialogTerminate",
            "WWiGuh9XfgQ#libSceErrorDialog":"sceErrorDialogUpdateStatus",
            "M2ZF-ClLhgY#libSceErrorDialog":"sceErrorDialogOpen",
            "Z4wwCFiBELQ#libSceNetCtl":"sceNetCtlTerm",
            "gky0+oaNM4k#libSceNetCtl":"sceNetCtlInit",
            "SsC-m-S9JTA#libSceSystemService":"sceSystemServiceParamGetString",
            "tPfQU2pD4-M#libSceSystemService":"sceSystemServiceShowDisplaySafeAreaSettings",
            "Idffwf3yh8s#libSceGnmDriver":"sceGnmDrawInitDefaultHardwareState",
            "lN7Gk-p9u78#libSceGnmDriver":"sceGnmSetSpiEnableSqCountersForUnitInstance",
            "0O3xxFaiObw#libSceGnmDriver":"sceGnmSetSpiEnableSqCounters",
            "N5KDtkIjjJ4#libSceVideoOut":"sceVideoOutUnregisterBuffers",
            "DYhhWbJSeRg#libSceVideoOut":"sceVideoOutColorSettingsSetGamma",
            "pv9CI5VC+R0#libSceVideoOut":"sceVideoOutAdjustColor",
            "zgXifHT9ErY#libSceVideoOut":"sceVideoOutIsFlipPending",
        }
        for nid,symbol in confirmed.items():
            self.assertIn(f'{{"{nid}","{symbol}"}}',names)
        for symbol in (
            "sceErrorDialogInitialize","sceErrorDialogOpen","sceErrorDialogUpdateStatus",
            "sceErrorDialogTerminate","sceNetCtlInit","sceNetCtlTerm",
            "sceSystemServiceParamGetString","sceSystemServiceShowDisplaySafeAreaSettings"
        ):
            self.assertIn(f'"{symbol}"',services)

    def test_confirmed_firstlight_service_imports(self):
        names=(ROOT/"src"/"import_names.inc").read_text(encoding="utf-8")
        content=(ROOT/"src"/"runtime_content.c").read_text(encoding="utf-8")
        services=(ROOT/"src"/"runtime_services.c").read_text(encoding="utf-8")
        expected={
            "m47juOmH0VE#libSceAppContent":"sceAppContentGetAddcontInfo",
            "aCYPMSUIaP8#libSceHttp":"sceHttpGetAllResponseHeaders",
            "yigr4V0-HTM#libSceHttp":"sceHttpSetRecvTimeOut",
            "xegFfZKBVlw#libSceHttp":"sceHttpSetSendTimeOut",
            "DK+GoXCNT04#libSceHttp":"sceHttpsLoadCert",
            "v6EZ-YWRdMs#libScePlayGo":"scePlayGoGetEta",
            "LosLlHOpNqQ#libScePlayGo":"scePlayGoSetLanguageMask",
            "Nn7zKwnA5q0#libScePlayGo":"scePlayGoGetToDoList",
        }
        for nid,symbol in expected.items():
            self.assertIn(f'{{"{nid}","{symbol}"}}',names)
        self.assertIn("DRM_NO_ENTITLEMENT",content)
        self.assertIn("addon_info",content)
        for symbol in (
            "sceHttpGetAllResponseHeaders","sceHttpSetRecvTimeOut","sceHttpSetSendTimeOut",
            "sceHttpsLoadCert","scePlayGoGetEta","scePlayGoSetLanguageMask","scePlayGoGetToDoList"
        ):
            self.assertIn(f'"{symbol}"',services)

    def test_safe_launcher_defaults(self):
        launcher=(ROOT/"run_firstlight.py").read_text(encoding="utf-8")
        for setting in (
            '"GOW3_NO_MSAA":"0"',
            '"GOW3_UPSCALER":"none"',
            '"GOW3_FSR1":"0"',
            '"GOW3_RCAS":"0"',
            '"GOW3_OBJECT_MOTION":"0"',
        ):
            self.assertIn(setting,launcher)
        self.assertNotIn('"--patches"',launcher)

    def test_build_emits_firstlight_probe(self):
        build=(ROOT/"build.sh").read_text(encoding="utf-8")
        self.assertIn("firstlight-probe.exe",build)
        self.assertIn("firstlight-probe",build)

if __name__=="__main__":
    unittest.main()
