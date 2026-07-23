import argparse
import sys
import os
import json

tools_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, tools_dir)

try:
    from spark.agent import SparkAgent
except ImportError:
    try:
        from .agent import SparkAgent
    except ImportError:
        import importlib.util

        spec = importlib.util.spec_from_file_location("agent", os.path.join(tools_dir, "spark", "agent.py"))
        agent_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(agent_module)
        SparkAgent = agent_module.SparkAgent


def main():
    parser = argparse.ArgumentParser(description="Spark Agent CLI - Automotive Software Configuration Assistant")
    parser.add_argument("--model", type=str, default="Qwen/Qwen3.5-35B-A3B", help="LLM model to use")
    parser.add_argument("--api-key", type=str, help="API key for LLM service")
    args = parser.parse_args()

    print("=" * 70)
    print("         SPARK Agent - Automotive Software Configuration Assistant")
    print("=" * 70)
    print(f"Model: {args.model}")
    print("-" * 70)
    print("Type '/help' for available commands")
    print("=" * 70)

    agent = SparkAgent(apiKey=args.api_key)
    agent.setModel(args.model)

    config_file = None
    schema_file = None

    while True:
        try:
            user_input = input("\nYou: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n\nGoodbye!")
            sys.exit(0)

        if not user_input:
            continue

        if user_input.startswith("/"):
            cmd = user_input.lower()
            if cmd == "/exit":
                print("Goodbye!")
                sys.exit(0)
            elif cmd == "/clear":
                agent.msgMgr.clear()
                agent.msgMgr.append({"role": "system", "content": agent.systemPrompt})
                print("[INFO] Chat history cleared")
            elif cmd == "/help":
                print("\nAvailable commands:")
                print("  /exit          - Exit the chat")
                print("  /clear         - Clear chat history")
                print("  /help          - Show this help")
                print("  /config <file> - Set config file path for context")
                print("  /schema <file> - Set schema file path for context")
                print("  /model <name>  - Change LLM model")
                print("  /info          - Show current settings")
            elif cmd.startswith("/config "):
                config_file = cmd[8:].strip()
                print(f"[INFO] Config file set to: {config_file}")
            elif cmd.startswith("/schema "):
                schema_file = cmd[8:].strip()
                print(f"[INFO] Schema file set to: {schema_file}")
            elif cmd.startswith("/model "):
                model_name = cmd[7:].strip()
                agent.setModel(model_name)
                print(f"[INFO] Model changed to: {model_name}")
            elif cmd == "/info":
                print(f"\nCurrent settings:")
                print(f"  Model: {agent.model}")
                print(f"  Config file: {config_file or 'None'}")
                print(f"  Schema file: {schema_file or 'None'}")
                print(f"  Messages in history: {len(agent.msgMgr.messages())}")
            else:
                print(f"[ERROR] Unknown command: {cmd}")
            continue

        full_message = user_input
        if config_file:
            full_message = f"Config file: {config_file}\n\n{full_message}"
        if schema_file:
            full_message = f"Schema file: {schema_file}\n\n{full_message}"

        print("\n[Thinking...]\n")

        response = agent.chat(full_message)

        parsed = json.loads(response)
        print("\n" + "=" * 70)
        print("RESPONSE")
        print("-" * 70)

        message = parsed.get("message", "")
        if message:
            print(f"\n{message}")

        issues = parsed.get("issues", [])
        if issues:
            print(f"\nIssues ({len(issues)}):")
            for idx, issue in enumerate(issues, 1):
                severity = issue.get("severity", "INFO")
                module = issue.get("module", "")
                desc = issue.get("description", "")
                print(f"\n{idx}. [{severity}] {module}: {desc}")

                suggestion = issue.get("suggestion", "")
                if suggestion:
                    print(f"   Suggestion: {suggestion}")

                changes = issue.get("changes", {})
                if changes:
                    sets = changes.get("sets", [])
                    deletions = changes.get("deletions", [])
                    if sets:
                        print(f"   Sets: {json.dumps(sets, indent=2, ensure_ascii=False)}")
                    if deletions:
                        print(f"   Deletions: {json.dumps(deletions, indent=2, ensure_ascii=False)}")

        print("\n" + "=" * 70)


if __name__ == "__main__":
    main()
