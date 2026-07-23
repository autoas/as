"""
Unit test for SPARK agent configuration validation.
Run from d:\repository\ssas\tools directory: python -m spark.test_spark
"""

import json
import sys
import os


def test_config_validation():
    """Test the configuration validation flow."""
    print("Initializing SparkAgent...")
    from spark.agent import SparkAgent
    agent = SparkAgent()
    
    if not agent.client:
        print("ERROR: LLM client not configured. Please check settings.json")
        return
    
    print("Testing configuration validation for jse.json...")
    request = "help validate config D:\\repository\\ssas\\tools\\json.editor\\jse.json"
    
    print(f"Sending request: {request}")
    response = agent.chat(request)
    
    print("\n=== RESPONSE ===")
    print(response)
    
    try:
        parsed = json.loads(response)
        if parsed.get("success") == False:
            print("\nERROR: Validation failed")
            print(f"Message: {parsed.get('message')}")
        else:
            print(f"\nSuccess: {parsed.get('success')}")
            print(f"Message: {parsed.get('message')}")
            issues = parsed.get("issues", [])
            if issues:
                print(f"\nFound {len(issues)} issues:")
                for i, issue in enumerate(issues):
                    print(f"\n{i+1}. {issue.get('description', '')}")
                    print(f"   Severity: {issue.get('severity', '')}")
                    print(f"   Module: {issue.get('module', '')}")
                    if issue.get('changes'):
                        print(f"   Changes: {json.dumps(issue['changes'], indent=2, ensure_ascii=False)}")
            else:
                print("\nNo issues found!")
    except json.JSONDecodeError:
        print("\nResponse is not valid JSON")


if __name__ == "__main__":
    test_config_validation()