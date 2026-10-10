{
  "variables": {
    "strut_include_dir%": "",
    "strut_embed_lib%": ""
  },
  "targets": [
    {
      "target_name": "strut_embed_addon",
      "sources": [ "addon.cc" ],
      "include_dirs": [ "<(strut_include_dir)" ],
      "cflags_cc": [ "-std=c++17", "-fexceptions" ],
      "conditions": [
        [ "OS=='win'", { "libraries": [ "<(strut_embed_lib)" ] } ]
      ]
    }
  ]
}
