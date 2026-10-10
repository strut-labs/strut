// FFI-9 C# consumer: P/Invokes the Strut embedding ABI (libstrut_embed) and drives the same
// corpus as the Python/Go/Node consumers. Struct declarations mirror include/strut/embed.h
// exactly (int, int64, double, int, {ptr,len}, ptr, fn, ctx). Callback delegates are rooted for
// the synchronous borrowed-callback call; errors and result values are released through the
// embedding library; retained callbacks hold the native single-owner contract.
using System;
using System.Runtime.InteropServices;

internal static class Embed
{
    public const int VOID = 0, BOOL = 1, INT = 2, FLOAT = 3, STRING = 4, BYTES = 5, RETAINED = 6, CALLBACK = 7;
    public const int ERR_INVOKE = 4;

    [StructLayout(LayoutKind.Sequential)]
    internal struct Value
    {
        public int kind;
        public long i;
        public double d;
        public int b;
        public IntPtr s_data;
        public ulong s_len;
        public IntPtr retained;
        public IntPtr cb_fn;
        public IntPtr cb_ctx;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeError
    {
        public int category;
        public IntPtr owner;
        public IntPtr type;
        public IntPtr message;
        public int code;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int CbTramp(IntPtr ctx, int x);

    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr strut_embed_context_create();
    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int strut_embed_context_destroy(IntPtr ctx);
    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int strut_embed_context_load_source(IntPtr ctx, byte[] source, ulong len, out IntPtr err);
    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int strut_embed_invoke(IntPtr ctx, byte[] name, Value[] args, ulong nargs, out Value outVal, out IntPtr err);
    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern int strut_embed_retained_invoke(IntPtr ctx, ref Value self, Value[] args, ulong nargs, out Value outVal, out IntPtr err);
    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void strut_embed_value_free(IntPtr ctx, ref Value v);
    [DllImport("strut_embed", CallingConvention = CallingConvention.Cdecl)]
    internal static extern void strut_embed_error_release(IntPtr ctx, IntPtr err);
}

internal static class Program
{
    static int failed;

    static void Check(bool ok, string what)
    {
        if (!ok) { Console.WriteLine("CSHARP CONSUMER FAIL: " + what); failed = 1; }
    }

    static IntPtr Err(string msg)
    {
        Console.WriteLine("CSHARP CONSUMER FAIL: " + msg);
        failed = 1;
        return IntPtr.Zero;
    }

    static byte[] Utf8(string s) => System.Text.Encoding.UTF8.GetBytes(s);

    static string Str(IntPtr p) => p == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(p);

    static Embed.Value Call(IntPtr ctx, string name, Embed.Value[] args, out string errCat, out string errMsg)
    {
        errCat = ""; errMsg = "";
        Embed.Value outVal = new Embed.Value();
        byte[] n = Utf8(name + "\0");
        IntPtr e;
        int rc = Embed.strut_embed_invoke(ctx, n, args, (ulong)args.Length, out outVal, out e);
        if (rc != 0)
        {
            if (e != IntPtr.Zero)
            {
                Embed.NativeError ne = Marshal.PtrToStructure<Embed.NativeError>(e);
                errCat = Str(ne.type); errMsg = Str(ne.message);
                Embed.strut_embed_error_release(ctx, e);
            }
            return new Embed.Value { kind = -1 };
        }
        return outVal;
    }

    static void Main()
    {
        // macOS dyld resolution for a local unsigned dylib is unreliable via short DllImport
        // names (consumer previously exited ~131/SIGQUIT). Resolve by absolute path from
        // STRUT_EMBED_LIB -- the same mechanism the Python consumer uses successfully -- and
        // fall back to the default loader when unset.
        NativeLibrary.SetDllImportResolver(typeof(Embed).Assembly, (name, asm, path) =>
        {
            if (name == "strut_embed")
            {
                string abs = Environment.GetEnvironmentVariable("STRUT_EMBED_LIB");
                if (!string.IsNullOrEmpty(abs) && System.IO.File.Exists(abs))
                    return NativeLibrary.Load(abs);
            }
            return IntPtr.Zero;
        });
        var SRC = string.Join("\n",
            "export \"C\" function add(int_32 a, int_32 b) -> int_32 { return a + b; }",
            "export \"C\" function addf(double_64 a, double_64 b) -> double_64 { return a + b; }",
            "export \"C\" function greet(string name) -> string { return \"hi \" + name; }",
            "export \"C\" function echo_bytes(bytes b) -> bytes { return b; }",
            "export \"C\" function apply_cb(function<(int_32)->int_32> f, int_32 v) -> int_32 { return f(v) + f(v + 1); }",
            "error EmbedErr { string message; }",
            "export \"C\" function risky(int_32 x) -> int_32 : EmbedErr { if (x < 0) { throw EmbedErr { message: \"boom\" }; } return x; }",
            "export \"C\" function make_rc(int_32 base) -> retained_callback<(int_32)->int_32> { return retained_callback((int_32 x) => x + base); }");

        IntPtr ctx = Embed.strut_embed_context_create();
        if (ctx == IntPtr.Zero) { Err("create"); return; }
        byte[] src = Utf8(SRC);
        IntPtr e;
        int lrc = Embed.strut_embed_context_load_source(ctx, src, (ulong)src.LongLength, out e);
        if (lrc != 0)
        {
            string lm = "";
            if (e != IntPtr.Zero)
            {
                Embed.NativeError ne = Marshal.PtrToStructure<Embed.NativeError>(e);
                lm = Str(ne.type) + "/" + Str(ne.message);
                Embed.strut_embed_error_release(ctx, e);
            }
            Err("load source rc=" + lrc + " " + lm);
            return;
        }

        Embed.Value r = Call(ctx, "add", new[] { I(20), I(22) }, out _, out _);
        Check(r.kind == Embed.INT && r.i == 42, "add=42");

        r = Call(ctx, "addf", new[] { F(2.5), F(3.25) }, out _, out _);
        Check(r.kind == Embed.FLOAT && Math.Abs(r.d - 5.75) < 1e-9, "addf=5.75");

        r = Call(ctx, "greet", new[] { S("stranger") }, out _, out _);
        if (r.s_data == IntPtr.Zero || r.s_len == 0) { Err("greet null result"); return; }
        byte[] gstr = new byte[r.s_len];
        Marshal.Copy(r.s_data, gstr, 0, (int)r.s_len);
        Check(System.Text.Encoding.UTF8.GetString(gstr) == "hi stranger", "greet");
        Free(ctx, r);

        byte[] payload = { 0x61, 0x00, 0x62, 0xFF };
        GCHandle pin = GCHandle.Alloc(payload, GCHandleType.Pinned);
        Embed.Value b = new Embed.Value { kind = Embed.BYTES, s_data = pin.AddrOfPinnedObject(), s_len = (ulong)payload.Length };
        r = Call(ctx, "echo_bytes", new[] { b }, out _, out _);
        if (r.s_data != IntPtr.Zero && r.s_len > 0)
        {
            byte[] back = new byte[r.s_len];
            Marshal.Copy(r.s_data, back, 0, (int)r.s_len);
            Check(back.Length == 4 && back[0] == 0x61 && back[1] == 0 && back[2] == 0x62 && back[3] == 0xFF, "bytes exact");
            Free(ctx, r);
        }
        else
        {
            Err("echo_bytes null result");
        }
        pin.Free();

        r = Call(ctx, "risky", new[] { I(-1) }, out string t, out string m);
        Check(r.kind == -1 && t == "EmbedErr" && m == "boom", "checked error EmbedErr/" + t + "/" + m);
        r = Call(ctx, "risky", new[] { I(3) }, out _, out _);
        Check(r.kind == Embed.INT && r.i == 3, "risky(3)=3");

        Embed.CbTramp cb = (IntPtr _x, int x) => x + 100;
        IntPtr cbPtr = Marshal.GetFunctionPointerForDelegate(cb);
        Embed.Value cv = new Embed.Value { kind = Embed.CALLBACK, cb_fn = cbPtr, cb_ctx = IntPtr.Zero };
        r = Call(ctx, "apply_cb", new[] { cv, I(5) }, out _, out _);
        Check(r.kind == Embed.INT && r.i == 211, "apply_cb=211");

        r = Call(ctx, "make_rc", new[] { I(100) }, out _, out _);
        Check(r.kind == Embed.RETAINED && r.retained != IntPtr.Zero, "make_rc retained");
        IntPtr handle = r.retained;
        Embed.Value self = new Embed.Value { kind = Embed.RETAINED, retained = handle };
        Embed.Value rc = new Embed.Value();
        int rrc2;
        IntPtr e2;
        rrc2 = Embed.strut_embed_retained_invoke(ctx, ref self, new[] { I(5) }, 1, out rc, out e2);
        Check(rrc2 == 0 && rc.i == 105, "retained before reload");

        Check(Embed.strut_embed_context_load_source(ctx, Utf8("export \"C\" function twice(int_32 x) -> int_32 { return x * 2; }"), 67, out e) == 0, "reload");
        rrc2 = Embed.strut_embed_retained_invoke(ctx, ref self, new[] { I(5) }, 1, out rc, out e2);
        Check(rrc2 == 0 && rc.i == 105, "retained after reload");

        Check(Embed.strut_embed_context_destroy(ctx) != 0, "BUSY destroy while retained outstanding");
        Embed.Value freeV = new Embed.Value { kind = Embed.RETAINED, retained = handle };
        Embed.strut_embed_value_free(ctx, ref freeV);
        Check(Embed.strut_embed_context_destroy(ctx) == 0, "destroy after release");

        if (failed != 0) { Console.WriteLine("csharp consumer FAILED"); }
        else { Console.WriteLine("csharp consumer ok"); }
        Environment.Exit(failed != 0 ? 1 : 0);

        static Embed.Value I(long x) => new Embed.Value { kind = Embed.INT, i = x };
        static Embed.Value F(double x) => new Embed.Value { kind = Embed.FLOAT, d = x };
        
        static Embed.Value S(string x) => new Embed.Value { kind = Embed.STRING, s_data = Marshal.StringToHGlobalAnsi(x), s_len = (ulong)x.Length };
        static void Free(IntPtr c, Embed.Value v) => Embed.strut_embed_value_free(c, ref v);
    }
}